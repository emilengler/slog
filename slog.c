/*
 * Copyright (c) 2022 Emil Engler <engler+slog@unveil2.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <lowdown.h>

struct markdown {
	struct lowdown_metaq	 metaq;
	char			*buf;
	size_t			 nbuf;
};

struct post {
	char	*id;
	char	*title;
	char	*date;
	char	*body;
};

struct template {
	char	*header;
	char	*item;
	char	*footer;
};

/* Fatal wrappers for libc functions. */
static FILE	*efopen(const char *, const char *);
static void	*emalloc(size_t);
static char	*estrdup(const char *);
static char	*estrndup(const char *, size_t);

/* Generic helper functions. */
static char	*aprintf(const char *, ...);
static char	*fmt_date(const char *, const char *);
static char	*read_file(const char *);

/* slog(1) specific functions. */
static void	 markdown_init(struct markdown *, FILE *);
static void	 markdown_free(struct markdown *);
static void	 post_init(struct post *, FILE *);
static void	 post_free(struct post *);
static void	 template_init(struct template *, const char *);
static void	 template_free(struct template *);

/* Global variables. */
static const char	*datefmt = "%Y-%m-%d %H:%M";

static FILE *
efopen(const char *path, const char *mode)
{
	FILE	*fp;

	if ((fp = fopen(path, mode)) == NULL)
		err(1, "fopen %s", path);

	return fp;
}

static void *
emalloc(size_t size)
{
	void	*data;

	if ((data = malloc(size)) == NULL)
		err(1, "malloc %zu", size);

	return data;
}

static char *
estrdup(const char *s)
{
	return estrndup(s, strlen(s));
}

static char *
estrndup(const char *s, size_t n)
{
	char	*str;

	if ((str = strndup(s, n)) == NULL)
		err(1, "strndup");

	return str;
}

static char *
aprintf(const char *fmt, ...)
{
	char	*str;
	va_list	 a1, a2;
	int	 sz;

	va_start(a1, fmt);
	va_copy(a2, a1);

	if ((sz = vsnprintf(NULL, 0, fmt, a1)) == -1)
		err(1, "vsnprintf");
	va_end(a1);

	str = emalloc(sz + 1);
	if (vsnprintf(str, sz + 1, fmt, a2) == -1)
		err(1, "vsnprintf");
	va_end(a2);

	return str;
}

static char *
fmt_date(const char *date, const char *fmt)
{
	char		*str;
	struct tm	 tm;

	if (strptime(date, fmt, &tm) == NULL)
		err(1, "strptime %s", date);

	str = emalloc(64);
	memset(str, '\0', 64);
	if (strftime(str, 63, datefmt, &tm) == 0)
		err(1, "strftime %s", date);

	return str;
}

static char *
read_file(const char *filename)
{
	FILE	*fp;
	char	*str;
	long	 sz;

	fp = efopen(filename, "r");

	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	str = emalloc(sz + 1);
	fread(str, 1, sz, fp);
	str[sz] = '\0';

	return str;
}

static void
markdown_init(struct markdown *md, FILE *fp)
{
	struct lowdown_opts	opts;

	memset(&opts, 0, sizeof(struct lowdown_opts));
	opts.type = LOWDOWN_HTML;
	opts.feat = LOWDOWN_AUTOLINK |
		LOWDOWN_COMMONMARK |
		LOWDOWN_FENCED |
		LOWDOWN_FOOTNOTES |
		LOWDOWN_METADATA |
		LOWDOWN_STRIKE |
		LOWDOWN_TABLES;
	opts.oflags = LOWDOWN_HTML_HEAD_IDS |
		LOWDOWN_HTML_OWASP |
		LOWDOWN_HTML_NUM_ENT |
		LOWDOWN_SMARTY;
	if (!lowdown_file(&opts, fp, &md->buf, &md->nbuf, &md->metaq))
		errx(1, "lowdown_file");
}

static void
markdown_free(struct markdown *md)
{
	lowdown_metaq_free(&md->metaq);
	free(md->buf);
}

static void
post_init(struct post *post, FILE *fp)
{
	struct lowdown_meta	*meta;
	struct markdown		 md;

	memset(post, 0, sizeof(struct post));
	markdown_init(&md, fp);

	if (md.nbuf == 0)
		errx(1, "missing body");

	/* Parse the document header. */
	TAILQ_FOREACH(meta, &md.metaq, entries) {
		/* TODO: Make this more elegant. */
		/* TODO: Fix potential memory leaks regarding duplicate keys. */
		if (strcmp(meta->key, "id") == 0)
			post->id = estrdup(meta->value);
		else if (strcmp(meta->key, "title") == 0)
			post->title = estrdup(meta->value);
		else if (strcmp(meta->key, "date") == 0)
			post->date = fmt_date(meta->value, "%F %R");
	}

	/*
	 * Simply pointing to md.buf is not possible, because there is no
	 * gurantee, that the string is NUL-terminated.
	 */
	post->body = estrndup(md.buf, md.nbuf);

	markdown_free(&md);
}

static void
post_free(struct post *post)
{
	free(post->id);
	free(post->title);
	free(post->date);
	free(post->body);
}

static void
template_init(struct template *tmplt, const char *path)
{
	char	*path_header, *path_item, *path_footer;

	path_header = aprintf("%s/header", path);
	path_item = aprintf("%s/item", path);
	path_footer = aprintf("%s/footer", path);

	tmplt->header = read_file(path_header);
	tmplt->item = read_file(path_item);
	tmplt->footer = read_file(path_footer);

	free(path_header);
	free(path_item);
	free(path_footer);
}

static void
template_free(struct template *tmplt)
{
	free(tmplt->header);
	free(tmplt->item);
	free(tmplt->footer);
}

int
main(int argc, char *argv[])
{
	return 0;
}
