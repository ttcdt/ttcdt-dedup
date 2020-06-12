/*

    ttcdt-dedup - Deduplicates files by using hard links.

    ttcdt <dev@triptico.com>

    This software is released into the public domain.

*/

#define VERSION "1.01"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>

struct entry {
    off_t size;
    char *fn;
};

struct entry *entries = NULL;
int n_entries = 0;
off_t min_size = 16;
int verbose = 1;
int dry_run = 0;


void fill_entries(char *globspec)
/* fills the entries struct recursively */
{
    glob_t globs;

    globs.gl_offs = 1;

    if (glob(globspec, GLOB_MARK, NULL, &globs) == 0) {
        int n;

        for (n = 0; globs.gl_pathv[n] != NULL; n++) {
            char *p = globs.gl_pathv[n];
            int z = strlen(p);

            if (p[z - 1] == '/') {
                /* subdirectory: glob there */
                char *sd = malloc(z + 2);
                strcpy(sd, p);
                strcat(sd, "*");

                fill_entries(sd);

                free(sd);
            }
            else {
                /* file: get size */
                struct stat st;

                if (stat(p, &st) != -1) {
                    if (st.st_size >= min_size) {
                        /* store this entry */
                        int i = n_entries;

                        n_entries++;
                        entries = realloc(entries, n_entries * sizeof(struct entry));

                        entries[i].size = st.st_size;
                        entries[i].fn   = strdup(p);
                    }
                }
                else {
                    printf("ERROR: stat() error for %s\n", p);
                }
            }
        }
    }

    globfree(&globs);
}


int entry_cmp(const void *v1, const void *v2)
/* entry sort callback */
{
    struct entry *e1 = (struct entry *)v1;
    struct entry *e2 = (struct entry *)v2;

    return e1->size > e2->size ? 1 : e1->size < e2->size ? -1 : 0;
}


void sort_entries(void)
/* sorts the entries by size */
{
    qsort(entries, n_entries, sizeof(struct entry), entry_cmp);
}


void dedup_entries(void)
/* iterates the entries, deduplicating if applicable */
{
    int n;
    struct entry *o = entries;

    for (n = 0; n < n_entries - 1; n++) {
        if (o->fn) {
            struct entry *l = o + 1;
            struct stat os;

            /* not expected to fail */
            stat(o->fn, &os);
    
            /* iterate all entries with the same size */
            while (l->size == o->size) {
                if (l->fn) {
                    struct stat ls;

                    stat(l->fn, &ls);
        
                    /* same device and *not* same inode? */
                    if (os.st_dev == ls.st_dev && os.st_ino != ls.st_ino) {
                        /* open files */
                        FILE *of;
        
                        if ((of = fopen(o->fn, "r"))) {
                            FILE *lf;

                            if ((lf = fopen(l->fn, "r"))) {
                                /* compare contents */
                                char ob[4096];
                                char lb[4096];
                                int z;
        
                                while ((z = fread(ob, 1, sizeof(ob), of))) {
                                    if (fread(lb, 1, z, lf) != z)
                                        break;
                                    else
                                    if (memcmp(ob, lb, z))
                                        break;
                                }
        
                                fclose(lf);
        
                                /* if no more data to read, files are identical */
                                if (z == 0) {
                                    if (verbose)
                                        printf("DEDUP: %s %s\n", o->fn, l->fn);

                                    if (!dry_run) {
                                        if (unlink(l->fn) == -1)
                                            printf("ERROR: unlink() error on %s\n", l->fn);
                                        else
                                        if (link(o->fn, l->fn) == -1)
                                            printf("ERROR: link() error on %s\n", l->fn);
                                    }

                                    /* ignore file from now on */
                                    l->fn = realloc(l->fn, 0);
                                }
                            }
                            else
                                printf("ERROR: cannot open %s\n", l->fn);

                            fclose(of);
                        }
                        else {
                            printf("ERROR: cannot open %s\n", o->fn);
                        }
                    }
                }

                l++;
            }
        }

        o++;
    }
}


int usage(void)
{
    printf("ttcdt-dedup %s - Deduplicates files by using hard links.\n", VERSION);
    printf("ttcdt <dev@triptico.com>\n");
    printf("This software is released into the public domain.\n\n");

    printf("Usage: ttcdt-dedup [-q] [-m {min_size}] {files...}\n\n");

    printf("Options:\n");
    printf(" -m {min_size}          Minimum file size to consider (in bytes).\n");
    printf(" -q                     Be quiet (only print errors).\n");
    printf(" -n                     Dry run (print what would be done, do nothing).\n");

    return 1;
}


int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc == 1)
        ret = usage();
    else {
        int n;

        for (n = 1; n < argc; n++) {
            if (strcmp(argv[n], "-m") == 0)
                min_size = atoi(argv[++n]);
            else
            if (strcmp(argv[n], "-q") == 0)
                verbose = 0;
            else
            if (strcmp(argv[n], "-n") == 0)
                dry_run = verbose = 1;
            else
                fill_entries(argv[n]);
        }

        if (n_entries == 0) {
            printf("WARN : no files\n");
            ret = 10;
        }
        else {
            sort_entries();

            dedup_entries();
        }
    }

    return ret;
}
