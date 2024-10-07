"""Do the checks and tasks that have to happen before doing a release.
"""

import io
import logging
import sys
import tempfile
import webbrowser


try:
    from readme_renderer.rst import render

    HAVE_README = True
except ImportError:
    HAVE_README = False

from zest.releaser import utils
from zest.releaser.utils import _execute_command


HTML_PREFIX = """<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  </head>
  <body>"""
HTML_POSTFIX = """
  </body>
</html>"""


logger = logging.getLogger(__name__)


def show_longdesc(headless):
    if not HAVE_README:
        logging.error(
            "To check the long description, we need the 'readme_renderer' "
            "package. "
            "(It is included if you install `zest.releaser[recommended]`)"
        )
        return 1

    filename = tempfile.mktemp(".html")
    # Note: for the setup.py call we use _execute_command() from our
    # utils module. This makes sure the python path is set up right.
    longdesc = _execute_command(utils.setup_py("--long-description"))
    warnings = io.StringIO()
    html = render(longdesc, warnings)
    if html is None:
        logging.error("Error generating html. Invalid ReST.")
        rst_filename = tempfile.mktemp(".rst")
        with open(rst_filename, "wb") as rst_file:
            rst_file.write(longdesc.encode("utf-8"))
        warning_text = warnings.getvalue()
        warning_text = warning_text.replace("<string>", rst_filename)
        print(warning_text)
        return 1

    if headless:
        return 0

    if "<html" not in html[:20]:
        # Add a html declaration including utf-8 indicator
        html = HTML_PREFIX + html + HTML_POSTFIX

    with open(filename, "wb") as fh:
        fh.write(html.encode("utf-8"))

    url = "file://" + filename
    logging.info("Opening %s in your webbrowser.", url)
    webbrowser.open(url)
    return 0


def main():
    parser = utils.base_option_parser()
    parser.add_argument(
        "--headless",
        action="store_true",
        dest="headless",
        default=False,
        help="Do not open a browser window with the HTML result")
    options = utils.parse_options(parser)
    utils.configure_logging()
    code = show_longdesc(headless=options.headless)
    sys.exit(code)
