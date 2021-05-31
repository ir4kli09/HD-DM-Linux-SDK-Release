#define  _POSIX_C_SOURCE 200808L
#include <linux/input.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>


#define READ_STATUS  	0
#define WRITE_STATUS 	1
#define EXCPT_STATUS 	2
/*
 * s    - fd
 * sec  - timeout seconds
 * usec - timeout microseconds
 * x    - select status
 */
extern int x_select(int s, int sec, int usec, int x);


static char *strmerge(const char *const s1, const char *const s2)
{
        const size_t     n1 = s1 ? strlen(s1) : 0;
        const size_t     n2 = s2 ? strlen(s2) : 0;
        char            *s;

        if (!n1 && !n2) {
                errno = EINVAL;
                return NULL;
        }

        s = (char *)malloc(n1 + n2 + 1);
        if (!s) {
                errno = ENOMEM;
                return NULL;
        }

        if (n1) memcpy(s, s1, n1);
        if (n2) memcpy(s + n1, s2, n2);
        s[n1 + n2] = 0;

        return s;
}

static int read_hex(const char *const filename)
{
        FILE            *in;
        unsigned int     value;

        in = fopen(filename, "rb");
        if (!in)
                return -1;

        if (fscanf(in, "%x", &value) == 1) {
                fclose(in);
                return (int)value;
        }

        fclose(in);
        return -1;
}

static int vendor_id(const char *const event)
{
        if (event && *event) {
                char    filename[256];
                int     result;

                result = snprintf(filename, sizeof(filename), "/sys/class/input/%s/device/id/vendor", event);
                if (result < 1 || result >= sizeof(filename))
                        return -1;

                return read_hex(filename);
        }
        return -1;
}

static int product_id(const char *const event)
{
        if (event && *event) {
                char    filename[256];
                int     result;

                result = snprintf(filename, sizeof(filename), "/sys/class/input/%s/device/id/product", event);
                if (result < 1 || result >= sizeof(filename))
                        return -1;

                return read_hex(filename);
        }
        return -1;
}

char *find_event(const int vendor, const int product)
{
        DIR             *dir;
        struct dirent   *cache, *entry;
        char            *name;
        long             maxlen;
        int              result;

        maxlen = pathconf("/sys/class/input/", _PC_NAME_MAX);
        if (maxlen == -1L)
                return NULL;

        dir = opendir("/sys/class/input");
        if (!dir)
                return NULL;

        cache = (struct dirent *)malloc(offsetof(struct dirent, d_name) + maxlen + 1);
        if (!cache) {
                closedir(dir);
                errno = ENOMEM;
                return NULL;
        }

        while (1) {

                entry = NULL;
                result = readdir_r(dir, cache, &entry);
                if (result) {
                        free(cache);
                        closedir(dir);
                        errno = result;
                        return NULL;
                }

                if (!entry) {
                        free(cache);
                        closedir(dir);
                        errno = ENOENT;
                        return NULL;
                }

                if (vendor_id(entry->d_name) == vendor) { // &&
                //    product_id(entry->d_name) == product) {
                        printf("entry-d_name...\n");
                        name = strmerge("/dev/input/", entry->d_name);
                        free(cache);
                        closedir(dir);
                        if (name)
                                return name;
                        errno = ENOMEM;
                        return NULL;
                }

        }
}

int getIntEventPath(int vendor_id, char *eventPath)
{
        int              status;
        char            *event;
        char             dummy;

        status = 0;
		event = find_event(vendor_id, 1234);
		if (event) {
				strcpy(eventPath, event);
				free(event);
		} else {
				fprintf(stderr, "Interrupt Path not found.\n");
				status = -1;
		}

		printf("eventPath: %s\n", eventPath);
        return status;
}

int getInterrupt(int fd)
{
	int rc = -1;
	struct input_event ev;

	if (fd < 0)
		return -1;

	read(fd, &ev, sizeof(ev));
	do {
		if (ev.type == 1 && ev.value == 1) {
			return 0;
		}
	} while(rc = read(fd, &ev, sizeof(ev)));
	return rc;
}
