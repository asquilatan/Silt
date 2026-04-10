<p align="center">
  <img src="icon/silt.png" />
</p>

# Silt

Silt is a small Git-like VCS project. The indexing/staging implementation is heavily inspired by **WYAG** (https://wyag.thb.lt/), and most of this structure comes from that model.

## What works now

1. `silt add` stages files into `.git/index`.
2. `silt ls-files` reads staged entries from the index.
3. `silt status` compares index vs worktree and shows staged/unstaged/untracked changes.
4. `silt commit -m "..."` builds tree objects from staged paths, writes a commit object, and updates the branch ref that `HEAD` points to.

## How the structure works

Silt uses the Git index version 2 layout: it writes the `DIRC` signature, version and entry count, then index entries containing file metadata, object SHA-1, flags, and path data with 8-byte alignment, and finally a SHA-1 checksum of the index content. Numeric fields are written/read in big-endian form so the index can be interpreted consistently.

Object storage follows Git-style SHA-based content addressing: file content is written as blob objects, index entries point to blob SHAs, commit creation builds nested tree objects from staged paths (directories + files), then writes a commit object that references the root tree (and parent when present). This is the core flow that allows interoperability with Git commands like `git status`, `git ls-files`, `git log`, and `git fsck`.

## Note from this implementation pass

I used GitHub Copilot CLI for the last debugging steps because I couldn’t figure out why the compatibility bug was happening during the final interop phase, specifically when `silt commit` results were being validated by Git (`git log`/`git fsck`) and the index/commit format mismatches still existed.
