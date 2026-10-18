# Source-only repository

This public copy keeps only the main branch and 8 tags, with game assets and build artifacts removed from history. The former branches were backed up separately before removal. Commit IDs differ from the original repository. The original repository was not modified.

The latest upstream refs could not be queried because GitHub authentication was unavailable. Branches preserved here are those already present in the original local checkout, including its remote-tracking branches.

## Development

Build and test in the original development checkout. Copy changed source files into the matching paths here, also reflecting added/deleted files and project configuration changes. Do not copy .git, game assets, or build outputs. Do not merge or pull the original repository into this filtered repository: doing so can reintroduce the original asset history.

This repository omits runtime game resources and prebuilt libraries. vcpkg dependencies must be restored and FMOD dependencies supplied separately for builds. Existing machine-specific paths may need adjustment. No build or game execution was tested here.

The main branch includes source-only ignore overrides and the latest project README.

## Upload to a NEW EMPTY repository

Run in PowerShell from this folder after creating a new empty GitHub/GitLab repository without an initial README or license commit:

    git remote add origin YOUR_NEW_REPOSITORY_URL
    git push origin main
    git push origin --tags

The public remote is https://github.com/aes011204/Expedition_33_Source.git and its default branch is main. Do not use the original team repository URL.

## Verification

All original locally available branch/tag tips were retained through filtering. 1,882 current source and shader files were compared against the original tree without content changes. git fsck passed after removal of the temporary original-object reference. Technical documentation that had not been committed in the original was copied here.

Migration details and the old/new commit mapping are stored locally under .git/filter-repo and .git/migration-*.json.
