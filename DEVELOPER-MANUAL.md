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
  - [Miscellaneous](#miscellaneous)
    - [Debug Messages](#debug-messages)
  - [Release Process](#release-process)
    - [Certificates](#certificates)
    - [Release Commit and Tag](#release-commit-and-tag)
    - [64-bit Windows Binary](#64-bit-windows-binary)
    - [32-bit Windows Binary](#32-bit-windows-binary)
    - [ARM64 Windows Binary](#arm64-windows-binary)
    - [macOS Binary](#macos-binary)
    - [Final steps](#final-steps)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## visualboyadvance-m Developer Manual

Here we will keep notes about our development process, policies and environment
setup guides.

### Issues Policies

#### Processing New Issues

Follow the following steps to process newly submitted issues:

- Edit the user's post to remove any links to illegal content such as ROM files.

- Edit the user's post to remove unused template sections etc.. Rephrase the
  issue title if it needs to be clarified.

- Label the issue as a question, bug or enhancement. This label can be changed
  later upon clarification if necessary.

- Add any other relevant labels, for example for the code subsystem.

- If it is strongly related to a work by a developer that you know of, assign
  them to the issue. If this is not the case, they can be unassigned.

- Ask the user for clarification of any details if needed.

#### Resolving Issues

- An issue is resolved by closing it in GitHub. A commit that fixes the issue
  should have the following line near the end of the body of the commit message:
```
Fix #999.
```
  This will automatically close the issue and assign the closing commit in the
  GitHub metadata when it is merged to master. The issue can be reopened if
  needed.

- A commit that is work towards resolving an issue, should have the issue number
  preceded by a pound sign either at the end of a commit message title, if it is
  of primary relevance to the issue, or the body otherwise.

### Pull Request and Commit Policies

#### Commit Message

Follow these guidelines always:

https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html

, the description of your work should be in the **commit message NOT the pull
request description**.

The title line must be no more than 50 characters and the description must be
wrapped at 72 characters. Most commit message editor interfaces will help you
with this. The title line must not end with a period.

Write everything in the imperative mood, e.g. change, fix, **NOT** changes,
changed, fixed, fixes etc..

A commit message must always have a title and a description, the description
must be independent of the title line, if necessary repeat the information in
the title line in the description.

The commit message must **ALL** changes, unless it's a minor refactor or
white space change or something and is not important.

The commit title line should be prefixed with an area, unless it involves the
wxWidgets GUI app, in which case it should **NOT** have a prefix.

If the commit is a user-facing functionality change or enhancement, the title
line of the commit must be a non-technical description of this change. For
example "Mute on speedup" because this will go into the changelog.

The text after the area prefix should not be capitalized.

Please use one of these area prefixes for non-main-GUI-app commits:

- doc: documentation, README.md etc..
- build: CMake, installdeps, preprocessor compatibility defines, compatibility
  headers, macOS build system etc..
- gb: the GameBoy emulator core or changes related to it.
- gba: the GameBoy Advance emulator core or changes related to it.
- libretro: the libretro core glue and build.
- sdl: anything for the SDL port, but **NOT** SDL functionality in the wxWidgets
  GUI app.
- translations: anything related to translations.

. Add other areas here if needed.

If a commit fixes a regression, use a title line such as:

```console
Fix regression <PROBLEM> in <SHORT-SHA>
```
, you can get the short SHA from `git log --oneline -100` or similar.

The commit description for a regression must refer to the breaking change in
reference format, which you can get from e.g. `git log --format=reference -20`.

#### Working on Pull Requests

When opening a pull request, push your branch to our repository if you were
given access, or a branch in your fork if you have not yet been given access. Do
**NOT** use `master`, use a specific branch.

If you are using a fork, set up your workflow like this:

```bash
git clone git@github.com:visualboyadvance-m/visualboyadvance-m
git remote add fork git@github.com:<your-GitHub-user>/visualboyadvance-m
git fetch --all --prune
git checkout -b your-work-branch-name
git commit -a --verbose --signoff -S
```
.

The `-S` flags tells Git to sign your commit with GnuPG. If you do not have a
GnuPG key, you will need to create one and upload it to a keyserver. I recommend
removing the password on your private key to not deal with `gpg-agent` and all
of this stuff.

All of this works fine on Windows, just install `GnuPG.GnuPG` from WinGet.

Your first push will then be:

```bash
git push -u fork HEAD
```
.

Subsequent commits will then be:

```bash
git commit -a --verbose --amend --reset-author --signoff -S
git push -f
```

. If you are a project member, then the workflow will be roughly:

```bash
git clone git@github.com:visualboyadvance-m/visualboyadvance-m
git fetch --all --prune
git checkout -b your-work-branch-name
git commit -a --verbose --signoff -S
```

. Your first push will be:

```bash
git push -u origin HEAD
```

. And subsequent pushes will be:

```bash
git commit -a --verbose --signoff -S --amend --reset-author
git push -f
```

. Please push frequently so that we can track your progress and review it.

Make sure the git history in your branch is clean and logical, edit when
necessary with `rebase -i`. In most cases a single commit with all of the
changes will be good. Sometimes you may want to split it up into multiple
logical commits for a large work, but a single commit is also fine if the title
line encapsulates all of the work for the changelog.

See the previous section on how to write commit messages.

If you are using Windows as your development environment, I recommend reading my
manual on Windows development environments
[here](https://github.com/rkitover/windows-dev-guide).

#### Collaboration on a Branch

To update when multiple people are working on a git branch, keep a couple of
things in mind:

- Always `push -f` unless you're adding a commit on top. And it's almost always
  better to edit the history than to add more commits. Never add commits fixing
  previous commits, only improving or adding to them.

- To update when someone else updated the branch with a `git push -f`:

```bash
git status # should be clean, with your work having been already pushed
git fetch --all --prune
git reset --hard origin/<work-branch-name>
```
.

- While actively working on a branch, keep it rebased on top of master, like
  this:

```bash
git fetch --all --prune
git rebase origin/master
git push -f
```

. You may sometimes need to fix conflicts, follow the instructions.

#### Commits from Admins

Maintainers and project members have the power to commit directly to master.
This power must be used responsibly.

Make your best attempt to follow these general guidelines:

- Things that are a minor fix or improvement that does not require discussion
  can be committed directly, keeping the following guidelines in mind.

- Bigger new features, code refactors and changes in architecture should go
  through the PR process.

- Absolutely **NEVER** `git push -f` on `master`. If you make a mistake, revert
  or push a fix commit.

- Push code changes to a branch first, so they can run through the CI. When you
  open the commit in GitHub there is a little icon in the upper left corner that
  shows the CI status for this commit. Differences in what different compilers
  allow is a problem that comes up **VERY** frequently. As well as
  incompatibilities between different configurations for both the C++ code and
  any supporting code. Once the CI is clear, you can merge your branch like
  this:

```bash
git push -f
git checkout master
git merge --ff-only <your-work-branch-name>
git push
git branch -D <your-work-branch-name>
git push origin ':refs/heads/your-work-branch-nbame'
```

. The last line there deletes the branch in our repository.

### Miscellaneous

#### Debug Messages

We have an override for `wxLogDebug()` to make it work even in non-debug builds
of wxWidgets and on windows, even in mintty.

It works like `printf()`, e.g.:

```cpp
int foo = 42;
wxLogDebug(wxT("the value of foo = %d"), foo);
```

From the core etc. the usual:

```cpp
fprintf(stderr, "...", ...);
```
, will work fine.

You need a debug build for this to work or to even have a console on Windows.
Pass `-DCMAKE_BUILD_TYPE=Debug` to CMake.

### Release Process

#### GnuPG Key

You will need to create a GnuPG key for signing your commits and release tags,
and upload it to a keyserver.

Make sure to install GnuPG on all environments where you will be making commits
and tags.

#### Certificates

Make sure you have set up a Windows code signing certificate with the right
password and a Mac 'Developer ID Application' certificate.

Put the Windows certificate into `~/.codesign/windows_comodo.pkcs12` as a PKCS12
file that is password protected, and put the password for it into
`~/.codesign/windows_comodo.pkcs12.password`.

#### Release Commit and Tag

Once you are sure you're ready to release, and you are in a git clone on master
with a clean working tree, use the cmake script to make the release commit and
tag:

```bash
mkdir build && cd build
cmake .. -DTAG_RELEASE=TRUE
```
, follow the instructions to edit the `CHANGELOG.md` and then push the release:

To reiterate, **make sure you edit the `CHANGELOG.md`** to remove any
non-user-facing changes before you make the release commit.

```bash
git push
git push --tags
```

If you don't want to push the release, to back out the change do:

```bash
cmake .. -DTAG_RELEASE=UNDO
```

#### 64-bit Windows Binary

For this you will preferably need the PowerShell environment setup described
[here](https://github.com/rkitover/windows-dev-guide), or by starting the `x64
Native Tools Command Prompt` from your Start Menu.

```powershell
mkdir build-msvc64
cd build-msvc64
cmake .. -DCMAKE_BUILD_TYPE=Release -DUPSTREAM_RELEASE=TRUE -G Ninja
ninja
```

Collect the following files for the release:

- `visualboyadvance-m-Win-x86_64.zip`
- `translations.zip`

Repeat the process for the debug build, with `-DCMAKE_BUILD_TYPE=Debug` and
collect this file:

- `visualboyadvance-m-Win-x86_64-debug.zip`
.

#### 32-bit Windows Binary

The 32-bit build is a legacy build for Windows XP compatibility. You will need
the MinGW toolchain to build it. The easiest method is to use the MINGW32 MSYS2
environment.

Make sure the Visual Studio `signtool.exe` is in your path, you can start MSYS2
with an inherited `PATH` from a Visual Studio enabled environment or add it to
your shell configuration.

First install dependencies with:

```bash
./installdeps
```
. Then build the 32-bit binary as follows:

```bash
mkdir build-mingw32
cd build-mingw32
cmake .. -DCMAKE_BUILD_TYPE=Release -DUPSTREAM_RELEASE=TRUE -G Ninja
ninja
```
. Collect this file for the release:

- `visualboyadvance-m-Win-x86_32.zip`

. Then repeat the process for the debug build with `-DCMAKE_BUILD_TYPE=Debug`,
and collect this file:

- `visualboyadvance-m-Win-x86_32-debug.zip`
.

#### ARM64 Windows Binary

You will need the MSVC ARM64 cross toolchain to build this binary, if you used
the install script from [here](https://github.com/rkitover/windows-dev-guide)
you will have it installed, otherwise run Visual Studio Installer and install
the component.

To enter the ARM64 cross environment, edit the PowerShell profile described
[here](https://github.com/rkitover/windows-dev-guide) or use the `vcvarsall.bat`
script with the `amd64_arm64` argument as described
[here](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170).

From there the process is the same as for the 64-bit build, collect the
following files for the release:

- `visualboyadvance-m-Win-arm64.zip`
- 'visualboyadvance-m-Win-arm64-debug.zip'
.

#### macOS Binary

Install the latest Xcode for your OS.

You will need bash from Homebrew/nix/MacPorts/whatever to run the build script.

You will need a codesigning certificate from Apple, which you will be able to
generate once you join their developer program from XCode. This is the
certificate of the type 'Developer ID Application' stored in your login
keychain.

If you are not using a GUI session, you will need to use a method to unlock your
login keychain before building so that your codesigning certificate can be used.
Adding the certificate and key to the System keychain is also a method that some
people use.

To unlock your keychain on login, you can add something like this to your
`~/.zshrc`:

```bash
security unlock-keychain -p "$(cat ~/.login-keychain-password)" login.keychain
```
, with your login password in that file.

For notarization to work, you will need to create an app-specific password on
https://appleid.apple.com , get your Team ID from your Apple Developer account,
and store them with this command:

```bash
xcrun notarytool store-credentials AC_PASSWORD \
               --apple-id you@domain.com \
               --team-id <DeveloperTeamID> \
               --password <secret_app_specific_2FA_password>
```
. Once all of this is set up, run:

```bash
tools/osx/builder
```
, this will take a while because it builds all of the dependencies.

Collect the following files from `~/vbam-build-mac-64bit/project`:

- `visualboyadvance-m-Mac-x86_64.zip`
- `visualboyadvance-m-Mac-x86_64-debug.zip`
.

#### Final steps

Go to the github releases tab, and make a release for the tag you pushed
earlier.

Put any notes to users and distro maintainers into the description as well as
the generated entries from `CHANGELOG.md` you edited earlier.

Upload all files collected during the earlier builds, the complete list is:


- `translations.zip`
- `visualboyadvance-m-Win-x86_64.zip`
- `visualboyadvance-m-Win-x86_64-debug.zip`
- `visualboyadvance-m-Win-x86_32.zip`
- `visualboyadvance-m-Win-x86_32-debug.zip`
- `visualboyadvance-m-Win-arm64.zip`
- 'visualboyadvance-m-Win-arm64-debug.zip'
- `visualboyadvance-m-Mac-x86_64.zip`
- `visualboyadvance-m-Mac-x86_64-debug.zip`

Update the winsparkle `appcast.xml` by running this cmake command:

```bash
cmake .. -DUPDATE_APPCAST=TRUE
```
, follow the instructions to push the change to the web data repo.

Announce the release on reddit r/emulation and the forum.
