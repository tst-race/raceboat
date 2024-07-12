from zest.releaser import git
from zest.releaser import utils

import logging
import os
import sys


logger = logging.getLogger(__name__)


def version_control():
    """Return an object that provides the version control interface.

    Base this on the detected version control system.

    We look for .git, .svn, etcetera in the current directory.  We
    might be in a directory a few levels down from the repository
    root.  So if we find nothing here, we go up a few directories.

    As safety valve we use a maximum to avoid an endless loop in case
    there is a logic error.
    """
    path = os.path.abspath(os.curdir)
    q = "You are NOT in the root of the repository. Do you want to go there?"
    for level in range(8):
        curdir_contents = os.listdir(path)
        if ".git" in curdir_contents:
            if level != 0 and utils.ask(q, default=False):
                os.chdir(path)
            return git.Git(path)
        # Get parent.
        newpath = os.path.abspath(os.path.join(path, os.pardir))
        if newpath == path:
            # We are at the system root.  We cannot go up anymore.
            break
        path = newpath

    logger.critical("No version control system detected.")
    sys.exit(1)
