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
#include <unistd.h>

#include <lowdown.h>

enum {
	HEADER,
	ITEM,
	FOOTER
};

struct markdown {
	struct lowdown_metaq	 metaq;
	char			*buf;
	size_t			 bufsz;
};

struct post {
	char		*id;
	char		*title;
	char		*datefmt;
	char		*daterss;
	struct tm	 date;
	char		*body;
};

/* Fatal wrappers for libc functions. */
static FILE		*efopen(const char *, const char *);
static void		*emalloc(size_t);
static char		*estrdup(const char *);
static char		*estrndup(const char *, size_t);
static void		 eunveil(const char *, const char *);

/* Generic helper functions. */
static char		*aprintf(const char *, ...);
static char		*astrftime(const char *, struct tm *);
static char		*read_file(const char *);
static __dead void	 usage(void);

/* slog(1) specific functions. */
static void		 markdown_init(struct markdown *, FILE *);
static void		 markdown_free(struct markdown *);
static void		 post_init(struct post *, FILE *, const char *);
static void		 post_free(struct post *);
static void		 read_template(char *[], const char *);
static void		 write_item(char *, struct post *, FILE *);
static void		 write_page(char *[], struct post[], size_t, FILE *);

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
estrdup(const char *str)
{
	char	*s;

	if ((s = strdup(str)) == NULL)
		err(1, "strdup %s", str);

	return s;
}

static char *
estrndup(const char *str, size_t n)
{
	char *s;

	if ((s = strndup(str, n)) == NULL)
		err(1, "strndup %s", str);

	return s;
}

static void
eunveil(const char *path, const char *permissions)
{
	if (unveil(path, permissions) == -1)
		err(1, "unveil %s", path);
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
astrftime(const char *fmt, struct tm *date)
{
	char	*str;

	str = emalloc(64);
	memset(str, '\0', 64);
	if (strftime(str, 63, fmt, date) == 0)
		err(1, "strftime");

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

static __dead void
usage(void)
{
	errx(1, "usage: %s [-d datefmt] [-o outputdir] [-t templatesdir] "
	     "posts...", getprogname());
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
	if (!lowdown_file(&opts, fp, &md->buf, &md->bufsz, &md->metaq))
		errx(1, "lowdown_file");
}

static void
markdown_free(struct markdown *md)
{
	lowdown_metaq_free(&md->metaq);
	free(md->buf);
}

static void
post_init(struct post *post, FILE *fp, const char *datefmt)
{
	struct lowdown_meta	*meta;
	struct markdown		 md;
	int			 has_date;

	markdown_init(&md, fp);

	if (md.bufsz == 0)
		errx(1, "missing body");

	/* Parse the metadata. */
	post->id = NULL;
	post->title = NULL;
	has_date = 0;
	TAILQ_FOREACH(meta, &md.metaq, entries) {
		if (strcmp(meta->key, "id") == 0 && post->id == NULL)
			post->id = estrdup(meta->value);
		else if (strcmp(meta->key, "title") == 0 && post->title == NULL)
			post->title = estrdup(meta->value);
		else if (strcmp(meta->key, "date") == 0 && !has_date) {
			has_date = 1;
			memset(&post->date, 0, sizeof(struct tm));
			if (strptime(meta->value, "%F %R", &post->date) == NULL)
				err(1, "strptime %s", meta->value);
		}
	}

	if (post->id == NULL || post->title == NULL || !has_date)
		errx(1, "missing required metadata");

	/* Format the dates. */
	post->datefmt = astrftime(datefmt, &post->date);
	post->daterss = astrftime("%a, %d %b %Y %T %Z", &post->date);

	/*
	 * Simply pointing to buf is not possible, because there is no gurantee
	 * that the string is NUL-terminated.
	 */
	post->body = estrndup(md.buf, md.bufsz);

	markdown_free(&md);
}

static void
post_free(struct post *post)
{
	free(post->id);
	free(post->title);
	free(post->datefmt);
	free(post->daterss);
	free(post->body);
}

static void
read_template(char *template[], const char *path)
{
	char	*paths[3];
	size_t	 i;

	paths[HEADER] = aprintf("%s/header", path);
	paths[ITEM] = aprintf("%s/item", path);
	paths[FOOTER] = aprintf("%s/footer", path);

	for (i = 0; i < 3; ++i) {
		template[i] = read_file(paths[i]);
		free(paths[i]);
	}
}

static void
write_item(char *template, struct post *post, FILE *fp)
{
	char	*key, *p, *q;

	p = template;
	while (*p != '\0') {
		if (p[0] == '$' && p[1] == '{') {
			p += 2;
			if ((q = strchr(p, '}')) == NULL)
				errx(1, "missing closing bracket");

			key = estrndup(p, q - p);
			if (strcmp(key, "id") == 0)
				fputs(post->id, fp);
			else if (strcmp(key, "title") == 0)
				fputs(post->title, fp);
			else if (strcmp(key, "datefmt") == 0)
				fputs(post->datefmt, fp);
			else if (strcmp(key, "daterss") == 0)
				fputs(post->daterss, fp);
			else if (strcmp(key, "body") == 0)
				fputs(post->body, fp);
			free(key);

			p = q + 1;
		} else
			fputc(*p++, fp);
	}
}

static void
write_page(char *template[], struct post posts[], size_t postssz, FILE *fp)
{
	size_t	i;

	fputs(template[HEADER], fp);
	for (i = 0; i < postssz; ++i)
		write_item(template[ITEM], posts + i, fp);
	fputs(template[FOOTER], fp);
}

int
main(int argc, char *argv[])
{
	char		**post_files, *datefmt, *output_dir, *templates_dir, ch;
	struct post	 *posts;
	FILE		 *fp;
	size_t		  postssz, i, j;

	/* Default values for the command line arguments. */
	datefmt = "%Y-%m-%d";
	output_dir = "./output";
	templates_dir = "./templates";

	while ((ch = getopt(argc, argv, "d:o:t:")) != -1) {
		switch (ch) {
		case 'd':
			datefmt = optarg;
			break;
		case 'o':
			output_dir = optarg;
			break;
		case 't':
			templates_dir = optarg;
			break;
		default:
			usage();
		}
	}
	if (argc == optind)
		usage();
	post_files = argv + optind;
	postssz = argc - optind;

	/* Sandbox the process. */
	eunveil(output_dir, "rwc");
	eunveil(templates_dir, "r");
	for (i = 0; i < postssz; ++i)
		eunveil(post_files[i], "r");
	if (pledge("stdio rpath wpath cpath", "") == -1)
		err(1, "pledge");

	/* Parse each and every post. */
	posts = emalloc(sizeof(struct post) * postssz);
	for (i = 0; i < postssz; ++i) {
		fp = efopen(post_files[i], "r");
		post_init(posts + i, fp, datefmt);
		fclose(fp);
	}

	/* Check for duplicate ids. */
	for (i = 0; i < postssz; ++i) {
		for (j = i + 1; j < postssz; ++j) {
			if (strcmp(posts[i].id, posts[j].id) == 0)
				errx(1, "duplicate id %s", posts[i].id);
		}
	}

	/* Clean everything up. */
	for (i = 0; i < postssz; ++i)
		post_free(posts + i);
	free(posts);

	return 0;
}
