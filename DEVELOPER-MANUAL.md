<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [visualboyadvance-m Developer Manual](#visualboyadvance-m-developer-manual)
  - [Issues Policies](#issues-policies)
    - [Processing New Issues](#processing-new-issues)
    - [Resolving Issues](#resolving-issues)
  - [Pull Request and Commit Policies](#pull-request-and-commit-policies)
    - [Commit Message](#commit-message)
    - [Collaboration on a Branch](#collaboration-on-a-branch)
    - [Commits from Maintainers](#commits-from-maintainers)
  - [Strings, Character Sets and Translations](#strings-character-sets-and-translations)
    - [Pulling Updated Translations](#pulling-updated-translations)
    - [Translations Message Catalog](#translations-message-catalog)
    - [Interaction with non-wxWidgets Code](#interaction-with-non-wxwidgets-code)
  - [Windows Native Development Environment Setup](#windows-native-development-environment-setup)
  - [Release Process](#release-process)
    - [Environment](#environment)
    - [Release Commit and Tag](#release-commit-and-tag)
    - [64-bit Windows Binary](#64-bit-windows-binary)
    - [32-bit Windows Binary](#32-bit-windows-binary)
    - [64-bit Mac Binary](#64-bit-mac-binary)
    - [Final steps](#final-steps)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## visualboyadvance-m Developer Manual

Here we will keep notes about our development process, policies and environment
setup guides.

### Issues Policies

#### Processing New Issues

Follow the following steps to process newly submitted issues:

- Edit the user's post to remove unused template sections etc.. Rephrase the
  issue title if it needs to be clarified.

- Label the issue as a question, bug or enhancement. This label can be changed
  later upon clarification if necessary.

- Add any other relevant labels, for example for the code subsystem.

- If it is strongly related to a work by a developer that you know of, assign
  them to the issue. If this is not the case, they can be unassigned.

- Ask the user for clarification of any details if needed.

#### Resolving Issues

- An issue is resolved by closing it in github. A commit that fixes the issue
  should have the following line near the end of the body of the commit message:
```
- Fix #999.
```
  This will automatically close the issue and assign the closing commit in the
  github metadata when it is merged to master. The issue can be reopened if
  needed.
- A commit that is work towards resolving an issue, should have the issue number
  preceded by a pound sign either at the end of a commit message title, if it is
  of primary relevance to the issue, or the body otherwise.

### Pull Request and Commit Policies

#### Commit Message

Follow these guidelines always:

https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html

the description of your work should be in the **commit message NOT the pull
request description**.

Make sure your git history is clean and logical, edit when necessary with
`rebase -i`.

#### Collaboration on a Branch

To update when multiple people are working on a git branch, keep a couple of
things in mind:

- Always `push -f` unless you're adding a commit on top. And it's almost always
  better to edit the history than to add more commits. Never add commits fixing
  previous commits, only improving or adding to them.

- To update when someone else (very rudely you might say) did a `push -f`, `pull
  --rebase` will **USUALLY** work. Verify the log, and if necessary do this
  instead:

```bash
git status # should be clean, with your work having been already pushed
git fetch --all --prune
git reset --hard origin/<branch-name>
```

While actively working on a branch, keep it rebased on top of master.

#### Commits from Maintainers

Maintainers have the power to commit directly to master. This power must be
used responsibly, something I fail to do myself often, and will try to improve
upon.

Make your most earnest attempt to follow these general guidelines to keep our
history clean:

- Things that are a straight fix or improvement that does not require discussion
  can be committed directly, keeping the following guidelines in mind.

- Bigger new features, code refactors and changes in architecture should go
  through the PR process.

- Push code changes to a branch first, so they can run through the CI.
  Differences in what different compilers allow is a problem that comes up
  **VERY** frequently. As well as incompatibilities between different
  configurations for both the C++ code and any supporting code.


### Strings, Character Sets and Translations

#### Pulling Updated Translations

Once in a while it is necessary to pull new and updated translations from
transifex.

For this you need the transifex client, available for Windows as well from
chocolatey as `transifex-client`.

To pull translations run:

```bash
tx pull -af
```

then check `git status` and if any message catalogs were updated, commit the
result as:

```bash
git commit -a --signoff -S -m'Transifex pull.'
git push
```

#### Translations Message Catalog

Strings that need to be translated by our wonderful translators on transifex
(thank you guys very much) need to be enclosed in `_("...")`, for example:

```cpp
wxLogError(_("error: something very wrong"));
```

The next time you run cmake after adding a string to be translated, the `.pot`
message catalog source will be regenerated, and you will see a loud message
telling you to push to transifex.

Strings in the XRC XML GUI definition files are automatically added to the
message catalog as well.

If you are working on a branch or a PR, don't push to transifex until it has
been merged to master.

Once it is, push it with:

```bash
tx push -s
```

#### Interaction with non-wxWidgets Code

Use our `UTF8(...)` function to force any `wxString` to UTF-8 for use by other
libraries, screen output or OS APIs. For example:

```cpp
fprintf(STDERR, "Error: %s\n", UTF8(err_msg));
```

There is one exception to this, when using `wxString::Printf()` and such, you
can't pass another `wxString` to the `%s` format directly, use something like
this:

```cpp
wxString err;
err.Printf("Cannot read file: %s", fname.wc_str());
```

this uses the `wchar_t` UTF-16 representation on Windows and does nothing
elsewhere.

For calling Windows APIs with strings, use the wide char `W` variants and the
`wc_str()` method as well.

### Windows Native Development Environment Setup

This guide has been moved to:

https://github.com/rkitover/windows-dev-guide

### Release Process

#### Environment

The variable `VBAM_NO_PAUSE`, if set, will cause cmake to not pause before gpg
signing operations, you want to set this if you've disabled your gpg passphrase
to not require interaction during release builds.

gpg set up with your key is helpful for the release process on all environments
where a binary is built, but you can also make the detached signature files
yourself at the end of the process.

For codesigning windows binaries, put your certificate into
`~/.codesign/windows_comodo.pkcs12`.

On Mac the 'Developer ID Application' certificate stored in your login keychain
is used, `keychain unlock` will prompt you for your login keychain password, to
avoid that set the `LOGIN_KEYCHAIN_PASSWORD` environment variable to your
password.

#### Release Commit and Tag

Once you are sure you're ready to release, and you are in a git clone on master
with a clean working tree, use the cmake script to make the release commit and
tag:

```bash
mkdir build && cd build
cmake .. -DTAG_RELEASE=TRUE
```

then push the release:

```bash
git push
git push --tags
```

If you don't want to push the release, to back out the change do:

```bash
cmake .. -DTAG_RELEASE=UNDO
```

#### 64-bit Windows Binary

For this you will preferably need the powershell environment setup described
earlier, however you can use a regular Visual Studio 64 bit native developer
command prompt as well.

```powershell
mkdir build
cd build
cmake .. -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release -DUPSTREAM_RELEASE=TRUE -G Ninja
ninja
```

Collect the following files for the release:

- `visualboyadvance-m-Win-64bit.zip`
- `visualboyadvance-m-Win-64bit.zip.asc`
- `translations.zip`
- `translations.zip.asc`

#### 32-bit Windows Binary

For this the optimal environment is a linux distribution with the mingw
toolchain, I use fedora.

You can set up a shell on a fedora distribution with docker as described here:

https://gist.github.com/rkitover/6379764c619c10e829e4b2fa0ae243fd

If using fedora, the cross script will install all necessary dependencies, if
not install the base toolchain (mingw gcc, binutils, winpthreads) using the
preferred method for your distribution, you can also use mxe for this.

https://mxe.cc/

```bash
sh tools/win/linux-cross-builder -32
```

You can also use msys2 on Windows, this is not recommended:

```bash
sh tools/win/msys2-builder -32
```

To set up msys2, see this guide:

https://gist.github.com/rkitover/d008324309044fc0cc742bdb16064454

Collect the following files from `~/vbam-build-mingw32/project` if using linux,
or `~/vbam-build-msys2-x86_64/project` if using msys2:

- `visualboyadvance-m-Win-32bit.zip`
- `visualboyadvance-m-Win-32bit.zip.asc`

#### 64-bit Mac Binary

Install the latest Xcode for your OS.

You will need bash and (optionally) gpg from homebrew (which you will also need
to install):

```bash
brew install bash gnupg
```

You will need a codesigning certificate from Apple, which you will be able to
generate once you join their developer program. This is the certificate of the
type 'Developer ID Application' stored in your login keychain. `keychain
unlock` will prompt you for your login keychain password, to avoid that set the
`LOGIN_KEYCHAIN_PASSWORD` environment variable to your password.

```bash
/usr/local/bin/bash tools/osx/builder -64
```

Collect the following files from `~/vbam-build-mac-64bit/project`:

- `visualboyadvance-m-Mac-64bit.zip`
- `visualboyadvance-m-Mac-64bit.zip.asc`

#### Final steps

Go to the github releases tab, and make a release for the tag you pushed
earlier.

Put any notes to users and distro maintainers into the description as well as
the entries from `CHANGELOG.md` generated earlier from git by the release
commit script.

Upload all files collected during the earlier builds, the complete list is:


- `translations.zip`
- `translations.zip.asc`
- `visualboyadvance-m-Mac-64bit.zip`
- `visualboyadvance-m-Mac-64bit.zip.asc`
- `visualboyadvance-m-Win-32bit.zip`
- `visualboyadvance-m-Win-32bit.zip.asc`
- `visualboyadvance-m-Win-64bit.zip`
- `visualboyadvance-m-Win-64bit.zip.asc`

Update the winsparkle appcast.xml by running this cmake command:

```bash
cmake .. -DUPDATE_APPCAST=TRUE
```

follow the instructions to push the change to the web data repo.

Announce the release on reddit r/emulation and the forum.
