#include <jitutils.h>


static int
jitutils_search(u8_t *src, u8_t *pat, size_t len, size_t max)
{
	int i, j;

	/* naive pattern search */
	for (i = 0; i < max; i++) {
        for (j = 0; j < len; j++) {
            if (src[i + j] != pat[j]) break;
		}
 
        if (j == len) return i;
	}

	return -1;
}

int jitutils_replace(u8_t *src, u8_t *orig, u8_t *replace, size_t len, size_t max)
{
	int i, pos;

	pos = jitutils_search(src, orig, len, max);
	if (pos == -1) return -1;

	for (i = 0; i < len; i++) {
		src[pos + i] = replace[i];
	}
	

	return pos;
}
