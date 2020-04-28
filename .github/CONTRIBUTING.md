Contributing to QGIS
====================

QGIS is an open source project and we appreciate contributions very much.

Proper formatting
-----------------

Before making a pull request, please make sure your code is properly formatted
by running the prepare commit script **before** issuing `git commit`.

    ./scripts/prepare_commit.sh

This can be automated by setting up the pre-commit hook properly.

    ln -s ../../scripts/prepare_commit.sh .git/hooks/pre-commit

Getting your pull request merged
--------------------------------

This is a volunteer project, so sometimes it may take a little while to merge
your pull request.

There is a [guide with hints for getting your pull requests merged](https://docs.qgis.org/testing/en/docs/developers_guide/git.html#pull-requests)
in the developers guide.
