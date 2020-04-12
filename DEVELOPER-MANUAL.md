<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

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
  - [PowerShell Notes](#powershell-notes)
  - [Release Process](#release-process)
    - [Environment](#environment)
    - [Release Commit and Tag](#release-commit-and-tag)
    - [64-bit Windows Binary](#64-bit-windows-binary)
    - [32-bit Windows Binary](#32-bit-windows-binary)
    - [64-bit Mac Binary](#64-bit-mac-binary)
    - [32-bit Mac Binary](#32-bit-mac-binary)
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

Install Visual Studio 2019:

You just want the core and C++ packages.

Install the chocolatey package manager:

- Press Win+X and open Windows PowerShell (administrator).

- Run this command:

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
```

Close the administrator PowerShell window and open it again.

Install some chocolatey packages:

```powershell
choco install ag dejavufonts git gpg4win hackfont microsoft-windows-terminal poshgit powershell-preview vim-tux zip unzip notepadplusplus openssh diffutils
```

Launch the terminal and choose Settings from the tab drop-down, this will open
the settings json in visual studio.

Add the Tango Dark Theme to the theme section:

```json
    {
      "background": "#000000",
      "black": "#000000",
      "blue": "#3465a4",
      "brightBlack": "#555753",
      "brightBlue": "#729fcf",
      "brightCyan": "#34e2e2",
      "brightGreen": "#8ae234",
      "brightPurple": "#ad7fa8",
      "brightRed": "#ef2929",
      "brightWhite": "#eeeeec",
      "brightYellow": "#fce94f",
      "cyan": "#06989a",
      "foreground": "#d3d7cf",
      "green": "#4e9a06",
      "name": "Tango Dark",
      "purple": "#75507b",
      "red": "#cc0000",
      "white": "#d3d7cf",
      "yellow": "#c4a000"
    }
```

Change the powershell profile like so:

```json
      {
        "acrylicOpacity": 0.5,
        "closeOnExit": true,
        "colorScheme": "Tango Dark",
        "commandline": "C:\\Program Files\\PowerShell\\7-preview\\pwsh.exe",
        "cursorColor": "#FFFFFF",
        "cursorShape": "filledBox",
        "fontFace": "Hack",
        "fontSize": 10,
        "guid": "{574e775e-4f2a-5b96-ac1e-a2962a402336}",
        "historySize": 30001,
        "icon": "ms-appx:///ProfileIcons/{574e775e-4f2a-5b96-ac1e-a2962a402336}.png",
        "backgroundImage": "ms-appdata:///roaming/wallhaven-127481.jpg",
        "backgroundImageOpacity": 0.32,
        "backgroundImageStretchMode": "uniformToFill",
        "name": "PowerShell Core",
        "padding": "0, 0, 0, 0",
        "snapOnInput": true,
        "startingDirectory": "%USERPROFILE%",
        "useAcrylic": false
      },
```

Notice I have a background image, if you want to install one, follow this
guide:

https://www.thomasmaurer.ch/2019/09/how-to-change-the-windows-terminal-background-image/

Change the settings for the tab shortcuts if you want, I use the same config as
kitty:

```json
            {
                "command" : "newTab",
                "keys" : 
                [
                    "ctrl+shift+t"
                ]
            },
            {
                "command" : "nextTab",
                "keys" : 
                [
                    "ctrl+shift+right"
                ]
            },
            {
                "command" : "prevTab",
                "keys" : 
                [
                    "ctrl+shift+left"
                ]
            },
```

This works kind of like a tmux for powershell!

The keys to copy a mouse selection are `Ctrl-Shift-C` like in kitty, to paste
`Ctrl-Shift-V`.

Now add some useful things to your powershell profile:

Run:

```powershell
notepad++ $profile
```

Here's mine, most importantly the Visual Studio environment setup:

```powershell
chcp 65001 > $null

Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

Set-Culture en-US

$env:EDITOR        = 'C:/Program\ Files/Vim/vim82/vim.exe'
$env:VBAM_NO_PAUSE = 1

function megs {
    ls $args | select Name, @{Name="MegaBytes"; Expression={$_.Length / 1MB}}
}

function cmconf {
    ag 'CMAKE_BUILD_TYPE|VCPKG_TARGET_TRIPLET' CMakeCache.txt
}

function sudo {
    ssh localhost $args
}

Set-Alias -name notepad -val "C:\Program Files\Notepad++\notepad++.exe"
Set-Alias -name which   -val Get-Command
Set-Alias -name grep    -val ag

# For vimdiff etc., install diffutils from choco.
if (Test-Path Alias:diff) {
    Remove-Item Alias:diff -force
}

# Load VS env only once.
if (-not $env:VSCMD_VER) {
    pushd "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build"
    cmd /c "vcvars64.bat & set" | where { $_ -match '=' } | %{
        $v = $_.split('=')
        Set-Item -Force -Path ("ENV:"+$v[0]) -Value $v[1]
    }
    popd
}

# Chocolatey profile
$ChocolateyProfile = "$env:ChocolateyInstall\helpers\chocolateyProfile.psm1"
if (Test-Path($ChocolateyProfile)) {
    Import-Module "$ChocolateyProfile"
}

Import-Module 'C:\tools\poshgit\dahlbyk-posh-git-9bda399\src\posh-git.psd1'

Import-Module PSReadLine

$HistoryFilePath = Join-Path ([Environment]::GetFolderPath('UserProfile')) .ps_history

Register-EngineEvent PowerShell.Exiting -Action { Get-History | Export-Clixml $HistoryFilePath } | out-null

if (Test-path $HistoryFilePath) { Import-Clixml $HistoryFilePath | Add-History }

Set-PSReadLineOption -EditMode Emacs

Set-PSReadlineKeyHandler -Key UpArrow   -Function HistorySearchBackward

Set-PSReadlineKeyHandler -Key DownArrow -Function HistorySearchForward
```

You may want to add an alias for your favorite editor.

Relaunch the terminal.

Add the powershell community extensions, to get a port of less and some other goodies:

```powershell
Find-Package pscx | Install-Package -Force -Scope CurrentUser -AllowClobber
```

and restart your shell.

Unrelated, but to get the up/down arrow history search behavior in bash, add
the following to your `~/.inputrc`:

```inputrc
## arrow up
"\e[A":history-search-backward
## arrow down
"\e[B":history-search-forward
```

To set notepad++ as the git commit editor, run this command:

```powershell
git config --global core.editor "'C:/Program Files/Notepad++/notepad++.exe' -multiInst -notabbar -nosession"
```

To configure truecolor support for vim, add this to your `~/.vimrc`:

```vim
if !has('gui-running')
  set bg=dark
  set termguicolors
endif
```

To use 256 color support instead, use this:

```vim
if !has('gui-running')
  set bg=dark
  set t_Co=256
endif
```

I also recommend adding this to spellcheck commit messages:

```vim
au BufRead COMMIT_EDITMSG setlocal spell
```

The most important thing that should be in your `~/.vimrc` is of course:

```vim
filetype plugin indent on
```

To use powershell as the vim internal shell instead of cmd, put this into `~/.vimrc`:

```vim
if (has('win32') || has('gui_win32')) && executable('pwsh')
    set shell=pwsh
    set shellcmdflag=\ -ExecutionPolicy\ RemoteSigned\ -NoProfile\ -Nologo\ -NonInteractive\ -Command
endif
```

To use neovim instead of vim, install the neovim chocolatey package and use this $profile setup:

```powershell
$env:EDITOR = 'C:/tools/neovim/Neovim/bin/nvim.exe'
Set-Alias -name vim -val nvim
```

To use gvim instead of console vim, you could try this `$profile` set up:

```powershell
$env:EDITOR = 'C:/Program\ Files/Vim/vim82/gvim.exe --servername main --remote-tab-silent'

function vim {
    & 'C:/Program Files/Vim/vim82/gvim.exe' --servername main --remote-tab-silent $args
}
```

Along with the code to save/restore window position from:

https://vim.fandom.com/wiki/Restore_screen_size_and_position

I also use an autocommand to keep the window vertically maximized despite tab
bar changes:

```vim
if has('gui_running')
  au BufEnter * set lines=999
endif
```

If you don't know how to use vim and want to learn, run `vimtutor`, it takes
about half an hour.

Now to set up gpg.

I recommend not using a passphrase so as not to deal with passphrase prompts,
which are a huge pain and break both ssh and release automation. If you have a
passphrase and want to remove it see:

http://www.peterscheie.com/unix/automating_signing_with_GPG.html

Configure git to use gpg4win:

```powershell
git config --global gpg.program "c:/Program Files (x86)/GnuPG/bin/gpg.exe"
```

Tell git to always sign commits:

```powershell
git config --global commit.gpgsign true
```

Now your development/build environment is ready!

To set up ssh into your powershell environment, which allows doing builds
remotely etc., edit the registry as described here to set powershell-preview as
the default shell:

https://github.com/PowerShell/Win32-OpenSSH/wiki/DefaultShell

Follow this guide to set up the server:

https://github.com/PowerShell/Win32-OpenSSH/wiki/Install-Win32-OpenSSH

### PowerShell Notes

PowerShell is very different from traditional UNIX shells, I am very new to it
myself, but I will pass on some tips here.

First, read this guide:

https://mathieubuisson.github.io/powershell-linux-bash/

You can use `ag` to both search and as a substitute for `grep`.

For example:

```powershell
alias | ag sort
```

Powershell itself provides a nice way to do simple grep/sed operations:

```powershell
alias | where { $_ -match '^se' } | select Name, ResolvedCommand
get-process | where { $_ -notmatch 'svchost' }
cmd /c date /T | %{ $_ -replace '.*(\d\d)/(\d\d)/(\d\d\d\d).*','$3-$1-$2' }
```

For `ls -ltr` use:

```powershell
ls | sort lastwritetime
```

You can use the `-Recurse` flag for `Get-ChildItem` (`ls`, `gci`) as a
substitute for `find`, e.g.:

```powershell
ls -rec *.xrc
```

Now combine this with the awesome powershell object pipeline to e.g. delete all
vim undo files:

```powershell
ls -rec .*un~ | rm
```

You will notice that `Remove-Item` (`rm`) does not take multiple space
separated items, you must separate them by commas, eg.:

```powershell
1..4 | %{ni foo$_; ni bar$_}
rm foo*, bar*
```

The equivalent of `rm -rf` to delete a directory is:

```powershell
rm -rec -for dir
```

The best replacement for `sudo` is to set up the openssh server with the shell
in the registry pointing to powershell-preview, commands run over ssh are
elevated. For example:

```powershell
ssh localhost choco upgrade all
```

the above `$profile` example has an alias for this.

It should not take you very long to learn enough basic usage for your dev
workflow. There is a lot of good info on powershell out there on blogs and
stackoverflow/superuser etc..

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

#### 32-bit Mac Binary

If using Mojave or later, you will need Xcode 9 installed side-by-side with
your OS Xcode, it should be installed to `/Applications/Xcode9.app`.  You can
obtain it from Apple developer downloads, or alternative sources which should
not be difficult to find.

Other requirements are the same as for the 64-bit binary.

```bash
/usr/local/bin/bash tools/osx/builder -32
```

Collect the following files from `~/vbam-build-mac-32bit/project`:

- `visualboyadvance-m-Mac-32bit.zip`
- `visualboyadvance-m-Mac-32bit.zip.asc`

#### Final steps

Go to the github releases tab, and make a release for the tag you pushed
earlier.

Put any notes to users and distro maintainers into the description as well as
the entries from `CHANGELOG.md` generated earlier from git by the release
commit script.

Upload all files collected during the earlier builds, the complete list is:


- `translations.zip`
- `translations.zip.asc`
- `visualboyadvance-m-Mac-32bit.zip`
- `visualboyadvance-m-Mac-32bit.zip.asc`
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
