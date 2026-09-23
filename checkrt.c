/*
 * checkrt tells an AppImage's AppRun which bundled C++ runtime libraries to put
 * in front of LD_LIBRARY_PATH.
 *
 * linuxdeployqt copies the build machine's libstdc++.so.6 and libgcc_s.so.1
 * into usr/optional/libstdc++/ and usr/optional/libgcc_s/. No RPATH points
 * there, so nothing loads them by default. A process can load only one copy of
 * each library, and a newer copy runs everything built for an older one, so the
 * right copy to load is always the newer of the bundled one and the system's.
 * AppRun runs this program with the usr/optional directory as its only
 * argument. The program prints the bundled directories whose library is newer
 * than the system's, separated by colons, and prints nothing when the system's
 * libraries are at least as new.
 *
 * The system's copy is the one the dynamic linker finds by name. The program
 * loads it with dlopen(), under the LD_LIBRARY_PATH the application itself will
 * get, so ld.so.cache, multiarch directories and custom paths are searched
 * exactly as they will be for the application. When the system has no copy,
 * the bundled one is used.
 *
 * The age of a library is read from its ELF version definitions, the
 * GLIBCXX_3.4.<n> and GCC_<x>.<y>.<z> names its symbols are tagged with, never
 * from its file name, which has not changed since GCC 3.4. The definitions are
 * read through the dynamic segment, the same table the dynamic linker reads, so
 * a library without section headers is read as well.
 *
 * The program needs nothing but the C library. Set APPIMAGE_CHECKRT_DEBUG=1 to
 * print each decision on standard error.
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if __SIZEOF_POINTER__ == 8
#define NATIVE_ELF_CLASS ELFCLASS64
#else
#define NATIVE_ELF_CLASS ELFCLASS32
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define NATIVE_ELF_DATA ELFDATA2LSB
#else
#define NATIVE_ELF_DATA ELFDATA2MSB
#endif

#define VERSION_PARTS 4

/* A dotted version such as 3.4.32; count is 0 when no version was read. */
typedef struct
{
    unsigned long parts[VERSION_PARTS];
    int count;
} Version;

typedef struct
{
    const char* directory; /* The directory below usr/optional. */
    const char* file;      /* The file name, which is also the library's soname. */
    const char* prefix;    /* The prefix of the version definitions to compare. */
} Runtime;

static const Runtime runtimes[] = {
    {"libstdc++", "libstdc++.so.6", "GLIBCXX_"},
    {"libgcc_s", "libgcc_s.so.1", "GCC_"},
};

static int debug = 0;

/* Returns 1 when a is newer than b. A missing version is older than any. */
static int is_newer(const Version* a, const Version* b)
{
    if (a->count == 0)
        return 0;
    if (b->count == 0)
        return 1;
    for (int i = 0; i < VERSION_PARTS; ++i) {
        if (a->parts[i] != b->parts[i])
            return a->parts[i] > b->parts[i];
    }
    return 0;
}

/* Writes a version as the definition name it came from, or "none". */
static void format_version(const Version* version,
                           const char* prefix,
                           char* buffer,
                           size_t size)
{
    size_t used = (size_t)
        snprintf(buffer, size, "%s", version->count ? prefix : "none");
    for (int i = 0; i < version->count && used < size; ++i)
        used += (size_t)
            snprintf(buffer + used, size - used, i ? ".%lu" : "%lu", version->parts[i]);
}

/*
 * Reads the version in a definition name such as GLIBCXX_3.4.32. Returns 0 when
 * the name does not start with the prefix followed by a digit, which leaves out
 * names such as GLIBCXX_LDBL_3.4 and the library's own soname entry.
 */
static int parse_version(const char* name, const char* prefix, Version* version)
{
    const size_t prefix_length = strlen(prefix);
    if (strncmp(name, prefix, prefix_length) != 0)
        return 0;

    const char* cursor = name + prefix_length;
    Version parsed = {{0}, 0};
    for (int i = 0; i < VERSION_PARTS; ++i) {
        parsed.count = i + 1;
        if (*cursor < '0' || *cursor > '9')
            return 0;
        while (*cursor >= '0' && *cursor <= '9') {
            if (parsed.parts[i] > 1000000UL)
                return 0;
            parsed.parts[i] = parsed.parts[i] * 10 + (unsigned long) (*cursor - '0');
            ++cursor;
        }
        if (*cursor == '\0')
            break;
        if (*cursor != '.')
            return 0;
        ++cursor;
    }
    if (*cursor != '\0')
        return 0;

    *version = parsed;
    return 1;
}

/* Copies a structure out of the file after checking that it lies inside. */
static int read_at(
    const unsigned char* data, size_t size, size_t offset, void* out, size_t out_size)
{
    if (offset > size || size - offset < out_size)
        return 0;
    memcpy(out, data + offset, out_size);
    return 1;
}

/* Turns a virtual address into a file offset through the PT_LOAD segments. */
static int address_to_offset(const unsigned char* data,
                             size_t size,
                             const ElfW(Ehdr) * header,
                             ElfW(Addr) address,
                             size_t* offset)
{
    for (size_t i = 0; i < header->e_phnum; ++i) {
        ElfW(Phdr) segment;
        if (!read_at(data,
                     size,
                     header->e_phoff + i * sizeof(segment),
                     &segment,
                     sizeof(segment)))
            return 0;
        if (segment.p_type == PT_LOAD && address >= segment.p_vaddr
            && address - segment.p_vaddr < segment.p_filesz) {
            *offset = segment.p_offset + (address - segment.p_vaddr);
            return 1;
        }
    }
    return 0;
}

/*
 * Finds the highest version definition with the given prefix in the ELF file
 * mapped at data. Returns 0 when the file is not a shared object of this
 * machine's ELF class or its tables are damaged, and 1 otherwise, including
 * when it defines no matching version.
 */
static int scan_version_definitions(const unsigned char* data,
                                    size_t size,
                                    const char* prefix,
                                    Version* highest)
{
    ElfW(Ehdr) header;
    if (!read_at(data, size, 0, &header, sizeof(header))
        || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
        || header.e_ident[EI_CLASS] != NATIVE_ELF_CLASS
        || header.e_ident[EI_DATA] != NATIVE_ELF_DATA
        || header.e_phentsize != sizeof(ElfW(Phdr)))
        return 0;

    /* Find the dynamic segment and read the tables it points to. */
    ElfW(Addr) strings_address = 0, definitions_address = 0;
    size_t strings_size = 0, definitions_count = 0;
    for (size_t i = 0; i < header.e_phnum; ++i) {
        ElfW(Phdr) segment;
        if (!read_at(data,
                     size,
                     header.e_phoff + i * sizeof(segment),
                     &segment,
                     sizeof(segment)))
            return 0;
        if (segment.p_type != PT_DYNAMIC)
            continue;
        for (size_t offset = segment.p_offset;
             offset - segment.p_offset + sizeof(ElfW(Dyn)) <= segment.p_filesz;
             offset += sizeof(ElfW(Dyn))) {
            ElfW(Dyn) entry;
            if (!read_at(data, size, offset, &entry, sizeof(entry)))
                return 0;
            if (entry.d_tag == DT_NULL)
                break;
            if (entry.d_tag == DT_STRTAB)
                strings_address = entry.d_un.d_ptr;
            else if (entry.d_tag == DT_STRSZ)
                strings_size = entry.d_un.d_val;
            else if (entry.d_tag == DT_VERDEF)
                definitions_address = entry.d_un.d_ptr;
            else if (entry.d_tag == DT_VERDEFNUM)
                definitions_count = entry.d_un.d_val;
        }
        break;
    }
    if (definitions_address == 0 || definitions_count == 0)
        return 1; /* No version definitions at all. */

    size_t strings_offset = 0, offset = 0;
    if (!address_to_offset(data, size, &header, strings_address, &strings_offset)
        || !address_to_offset(data, size, &header, definitions_address, &offset)
        || strings_offset > size || size - strings_offset < strings_size)
        return 0;
    const char* strings = (const char*) data + strings_offset;

    /* Each definition names its version in the first of its auxiliary entries. */
    for (size_t i = 0; i < definitions_count; ++i) {
        ElfW(Verdef) definition;
        if (!read_at(data, size, offset, &definition, sizeof(definition)))
            return 0;
        if (!(definition.vd_flags & VER_FLG_BASE) && definition.vd_cnt > 0) {
            ElfW(Verdaux) auxiliary;
            if (!read_at(data,
                         size,
                         offset + definition.vd_aux,
                         &auxiliary,
                         sizeof(auxiliary)))
                return 0;
            if (auxiliary.vda_name < strings_size
                && memchr(strings + auxiliary.vda_name,
                          '\0',
                          strings_size - auxiliary.vda_name)) {
                Version version;
                if (parse_version(strings + auxiliary.vda_name, prefix, &version)
                    && is_newer(&version, highest))
                    *highest = version;
            }
        }
        if (definition.vd_next == 0)
            break;
        offset += definition.vd_next;
    }
    return 1;
}

/* Reads the highest version with the given prefix that the file at path defines. */
static int read_highest_version(const char* path, const char* prefix, Version* highest)
{
    highest->count = 0;

    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
        return 0;
    const off_t end = lseek(descriptor, 0, SEEK_END);
    if (end <= 0) {
        close(descriptor);
        return 0;
    }
    const size_t size = (size_t) end;
    void* data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, descriptor, 0);
    close(descriptor);
    if (data == MAP_FAILED)
        return 0;

    const int result = scan_version_definitions(data, size, prefix, highest);
    munmap(data, size);
    return result;
}

/*
 * Finds the system's copy of a library the way the application would find it
 * without the bundled directories. Returns NULL when there is none.
 */
static const char* find_system_library(const char* file)
{
    void* handle = dlopen(file, RTLD_LAZY | RTLD_LOCAL);
    if (handle == NULL) {
        if (debug)
            fprintf(stderr, "checkrt: the system has no %s: %s\n", file, dlerror());
        return NULL;
    }
    struct link_map* map = NULL;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || map == NULL || map->l_name == NULL
        || map->l_name[0] == '\0')
        return NULL;
    return map->l_name;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <AppDir>/usr/optional\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char* optional_directory = argv[1];
    const char* debug_value = getenv("APPIMAGE_CHECKRT_DEBUG");
    debug = debug_value != NULL && debug_value[0] != '\0'
            && strcmp(debug_value, "0") != 0;

    int printed = 0;
    for (size_t i = 0; i < sizeof(runtimes) / sizeof(runtimes[0]); ++i) {
        const Runtime* runtime = &runtimes[i];

        char bundled_path[4096];
        const int length = snprintf(bundled_path,
                                    sizeof(bundled_path),
                                    "%s/%s/%s",
                                    optional_directory,
                                    runtime->directory,
                                    runtime->file);
        if (length < 0 || (size_t) length >= sizeof(bundled_path))
            continue;

        /* An unreadable bundled copy is never used; the system's stays. */
        Version bundled;
        if (access(bundled_path, R_OK) != 0)
            continue;
        if (!read_highest_version(bundled_path, runtime->prefix, &bundled)
            || bundled.count == 0) {
            if (debug)
                fprintf(stderr,
                        "checkrt: no %s* version in %s, not using it\n",
                        runtime->prefix,
                        bundled_path);
            continue;
        }

        /* A system copy whose version cannot be read counts as having none. */
        Version system = {{0}, 0};
        const char* system_path = find_system_library(runtime->file);
        if (system_path != NULL
            && !read_highest_version(system_path, runtime->prefix, &system) && debug)
            fprintf(stderr, "checkrt: cannot read the versions of %s\n", system_path);

        const int use_bundled = is_newer(&bundled, &system);
        if (debug) {
            char bundled_text[96], system_text[96];
            format_version(&bundled,
                           runtime->prefix,
                           bundled_text,
                           sizeof(bundled_text));
            format_version(&system, runtime->prefix, system_text, sizeof(system_text));
            fprintf(stderr,
                    "checkrt: %s: bundled %s, system %s (%s), using the %s copy\n",
                    runtime->file,
                    bundled_text,
                    system_text,
                    system_path != NULL ? system_path : "not found",
                    use_bundled ? "bundled" : "system's");
        }
        if (use_bundled) {
            printf("%s%s/%s",
                   printed ? ":" : "",
                   optional_directory,
                   runtime->directory);
            printed = 1;
        }
    }
    if (printed)
        putchar('\n');
    return EXIT_SUCCESS;
}
