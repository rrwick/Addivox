"""
mkdocs-with-pdf's fix_image_alignment() reads an <img>'s existing inline
style via getattr(img, 'style', ''), but BeautifulSoup tags don't expose
HTML attributes that way (the real accessor is img['style']), so getattr
always returns '' and the function overwrites the style attribute with an
empty string -- silently discarding any width/max-width set on an image.

This re-implements the function with that one line fixed, and installs it
in place of the original the plugin uses.
"""
import os
import re

from mkdocs_with_pdf import generator as _with_pdf_generator
from mkdocs_with_pdf.options import Options as _WithPdfOptions


def _parse_style(style_string):
    styles = {}
    if style_string:
        for attr in style_string.split(';'):
            if not len(attr):
                continue
            val = attr.split(':', 2)
            styles[val[0].strip()] = val[1].strip()
    return styles


def _convert_dimension(dim_str):
    if dim_str.isdecimal():
        return dim_str + 'px'
    return dim_str


def _fixed_fix_image_alignment(soup, logger=None):
    for img in soup.select('img'):
        try:
            if img.has_attr('class') and 'twemoji' in img['class']:
                continue

            styles = _parse_style(img.get('style', ''))

            if img.has_attr('align'):
                if img['align'] == 'left':
                    styles['float'] = 'left'
                    styles['padding-right'] = '1rem'
                    styles['padding-bottom'] = '0.5rem'
                    img.attrs.pop('align')
                elif img['align'] == 'right':
                    styles['float'] = 'right'
                    styles['padding-left'] = '1rem'
                    styles['padding-bottom'] = '0.5rem'
                    img.attrs.pop('align')

            if img.has_attr('width'):
                styles['width'] = _convert_dimension(img['width'])
                img.attrs.pop('width')
            if img.has_attr('height'):
                styles['height'] = _convert_dimension(img['height'])
                img.attrs.pop('height')

            img['style'] = " ".join(f'{k}: {v};' for k, v in styles.items())
        except Exception as e:
            if logger:
                logger.warning(f'Failed to convert img align: {e}')


_with_pdf_generator.fix_image_alignment = _fixed_fix_image_alignment


# Add the current version (parsed from release_notes.md's most recent
# entry, e.g. "### [Addivox v1.0.1 - 11 July 2026](...)") to the PDF cover.
def _get_addivox_version():
    release_notes_path = os.path.join(
        os.path.dirname(__file__), 'docs', 'release_notes.md')
    with open(release_notes_path, encoding='utf-8') as f:
        text = f.read()
    match = re.search(r'^### \[Addivox v([\d.]+)', text, re.MULTILINE)
    return match.group(1) if match else None


_addivox_version = _get_addivox_version()

if _addivox_version:
    _WithPdfOptions.cover_subtitle = property(
        lambda self: f'version {_addivox_version} documentation')
