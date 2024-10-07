# GPL, (c) Reinout van Rees
#
# Script to show the diff with the last relevant tag.

from zest.releaser import utils
from zest.releaser.utils import execute_command

import logging
import sys
import zest.releaser.choose


logger = logging.getLogger(__name__)


def main():
    utils.configure_logging()
    vcs = zest.releaser.choose.version_control()
    if len(sys.argv) > 1:
        found = sys.argv[-1]
    else:
        found = utils.get_last_tag(vcs)
    name = vcs.name
    full_tag = vcs.tag_url(found)
    logger.debug(
        "Picked tag '%s' for %s (currently at '%s').", full_tag, name, vcs.version
    )
    logger.info("Showing differences from the last commit against tag %s", full_tag)
    diff_command = vcs.cmd_diff_last_commit_against_tag(found)
    print(diff_command)
    print(execute_command(diff_command))
