#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Checks README.md's links to the hosted site against a site staged by
#  tools/build_site.sh, so the README never points at a page the site does not
#  serve:
#
#      python3 -I tools/check_site_links.py README.md site/
#
#  It fails (exit 1, one "check_site_links.py: ..." line per problem) unless:
#  - every link to SITE_URL is a page of the staged site: the path under it
#    ("" is index.html) a file there, a control=<Name> or demo=<Name> in its
#    query one of the staged gallery's picker entries (controltest.html's
#    <option value="...">, which are ControlTest --list; the demo shell's pages
#    are among them, and OpenGLCore --demo takes the same names), and no
#    parameter but that and scale=;
#  - README.md links to SITE_URL itself (its "Try it live" link);
#  - every image under docs/images/ in README.md is a link to the site: the
#    hero, demo.png, to demo.html, and a gallery image <Name>.png to
#    controltest.html?control=<Name>;
#  - the gallery images are tools/capture_samples.sh's SAMPLES, in that order
#    (the script's own rule: README.md embeds each of them, in its order).
#
#  tools/ci_site.sh runs it after staging the site, so the Pages workflow does.

import html.parser
import os
import re
import sys
import urllib.parse

SITE_URL = 'https://caseymcc.github.io/gweni/'

# the pages a query may carry a page name on, and the parameter it is in
PAGE_PARAMS = {
    'controltest.html': 'control',
    'demo.html': 'demo',
}
# what else a link may set: anything more is likely a typo of a page's argv
OTHER_PARAMS = {'scale'}

problems = []


def problem(text):
    problems.append(text)
    print('check_site_links.py: ' + text, file=sys.stderr)


class OptionValues(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.values = []

    def handle_starttag(self, tag, attrs):
        if tag == 'option':
            value = dict(attrs).get('value')
            if value:
                self.values.append(value)


def picker_entries(site):
    path = os.path.join(site, 'controltest.html')
    try:
        page = open(path, encoding='utf-8').read()
    except OSError as error:
        sys.exit('check_site_links.py: cannot read %s: %s (is %s a site staged by tools/build_site.sh?)'
                 % (path, error.strerror, site))
    parser = OptionValues()
    parser.feed(page)
    if not parser.values:
        sys.exit('check_site_links.py: %s has no picker entries' % path)
    return set(parser.values)


def capture_samples(repo):
    path = os.path.join(repo, 'tools', 'capture_samples.sh')
    script = open(path, encoding='utf-8').read()
    match = re.search(r'^SAMPLES=\(\n(.*?)^\)', script, re.M | re.S)
    if not match:
        sys.exit('check_site_links.py: no SAMPLES=( ... ) list in %s' % path)
    return [line.strip() for line in match.group(1).splitlines()
            if line.strip() and not line.strip().startswith('#')]


def check_link(url, site, entries):
    """The staged page url names, or None (and a problem) if there is none."""
    parts = urllib.parse.urlsplit(url)
    page = parts.path[len(urllib.parse.urlsplit(SITE_URL).path):] or 'index.html'
    if parts.fragment:
        problem('%s: a fragment the site has no anchor for' % url)
    if not page.endswith('.html') or not os.path.isfile(os.path.join(site, page)):
        problem('%s: %s is not a page of the staged site %s' % (url, page, site))
        return None
    query = urllib.parse.parse_qs(parts.query, keep_blank_values=True)
    allowed = PAGE_PARAMS.get(page)
    for key, values in query.items():
        if allowed is None or (key != allowed and key not in OTHER_PARAMS):
            problem('%s: %s takes no %s= from a README link' % (url, page, key))
            continue
        if key != allowed:
            continue
        for value in values:
            if value not in entries:
                problem('%s: %s is not in the gallery\'s picker (ControlTest --list)' % (url, value))
    return page, query


def main():
    if len(sys.argv) != 3:
        sys.exit('usage: check_site_links.py <README.md> <staged site dir>')
    readme_path, site = sys.argv[1], sys.argv[2]
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    readme = open(readme_path, encoding='utf-8').read()
    entries = picker_entries(site)

    # every link into the site
    urls = re.findall(re.escape(SITE_URL) + r'[^\s)\]>"\']*', readme)
    if SITE_URL not in urls:
        problem('README.md has no link to %s itself (its "Try it live" link)' % SITE_URL)
    for url in sorted(set(urls)):
        check_link(url, site, entries)

    # every image of docs/images/, and the link around it
    linked = {m.start(1): m.group(2)
              for m in re.finditer(r'\[(!\[[^\]]*\]\(docs/images/[^)]+\))\]\(([^)]+)\)', readme)}
    gallery = []
    for m in re.finditer(r'!\[[^\]]*\]\(docs/images/([^)]+)\)', readme):
        image = m.group(1)
        name, ext = os.path.splitext(image)
        target = linked.get(m.start())
        if target is None or not target.startswith(SITE_URL):
            problem('docs/images/%s is not a link to its page on %s' % (image, SITE_URL))
            continue
        parts = urllib.parse.urlsplit(target)
        query = urllib.parse.parse_qs(parts.query)
        page = parts.path[len(urllib.parse.urlsplit(SITE_URL).path):]
        if name == 'demo':
            if page != 'demo.html':
                problem('docs/images/demo.png, the hero, links to %s, not to demo.html' % target)
            continue
        gallery.append(name)
        if page != 'controltest.html' or query.get('control') != [name]:
            problem('docs/images/%s links to %s, not to controltest.html?control=%s' % (image, target, name))

    samples = capture_samples(repo)
    if gallery != samples:
        problem('README.md\'s gallery images are %s, tools/capture_samples.sh\'s SAMPLES %s: '
                'they must be the same, in the same order' % (' '.join(gallery), ' '.join(samples)))

    if problems:
        sys.exit(1)
    print('check_site_links.py: %d links to %s, all pages of %s; %d gallery images, as SAMPLES'
          % (len(urls), SITE_URL, site, len(gallery)))


main()
