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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void	markdown_init(struct markdown *md, FILE *fp);
static void	markdown_free(struct markdown *md);

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

int
main(int argc, char *argv[])
{
	return 0;
}
