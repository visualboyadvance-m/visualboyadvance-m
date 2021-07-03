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
    - [Install Chocolatey and Some Packages](#install-chocolatey-and-some-packages)
    - [Chocolatey Usage Notes](#chocolatey-usage-notes)
    - [Configure the Terminal](#configure-the-terminal)
    - [Setting up Vim](#setting-up-vim)
    - [Set up PowerShell Profile](#set-up-powershell-profile)
    - [PowerShell Usage Notes](#powershell-usage-notes)
    - [Miscellaneous](#miscellaneous)
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

#### Install Chocolatey and Some Packages

Make sure developer mode is turned on in Windows settings, this is necessary for
making unprivileged symlinks.

- Press Win+X and open Windows PowerShell (administrator).

- Run these commands:

```powershell
Set-ExecutionPolicy -Scope LocalMachine -Force RemoteSigned
iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
```

Close the administrator PowerShell window and open it again.

Install some chocolatey packages:

```powershell
choco install -y visualstudio2019community --params '--locale en-US'
choco install -y visualstudio2019-workload-nativedesktop
choco install -y hackfont dejavufonts ripgrep git gpg4win microsoft-windows-terminal powershell-core vim neovim zip unzip notepadplusplus diffutils ntop.portable grep gawk sed less transifex-client
choco install -y openssh --params '/SSHServerFeature /SSHAgentFeature /PathSpecsToProbeForShellEXEString:$env:programfiles\PowerShell\*\pwsh.exe'
```

#### Chocolatey Usage Notes

Here are some commands for using the Chocolatey package manager.

To search for a package:

```powershell
choco search patch
```

To get the description of a package:

```powershell
choco info patch
```

To install a package:

```powershell
choco install -y patch
```

To uninstall a package:


```powershell
choco uninstall -y patch
```

To list installed packages:

```powershell
choco list --local
```

To update all installed packages:

```powershell
choco update -y all
```

#### Configure the Terminal

Launch the terminal and choose Settings from the tab drop-down, this will open
the settings json in visual studio.

In the global settings, above the `"profiles"` section, add:

```json
// If enabled, selections are automatically copied to your clipboard.
"copyOnSelect": true,
// If enabled, formatted data is also copied to your clipboard
"copyFormatting": true,
"useTabSwitcher": false,
"wordDelimiters": " ",
"largePasteWarning": false,
"multiLinePasteWarning": false,
```

In the `"profiles"` `"defaults"` section add:

```json
"defaults":
{
    // Put settings here that you want to apply to all profiles.
    "fontFace": "Hack",
    "fontSize": 10,
    "antialiasingMode": "cleartype",
    "cursorShape": "filledBox",
    "colorScheme": "Tango Dark",
    "closeOnExit": "always"
},
```

I prefer `IBM Plex Mono` which you can install from:

https://github.com/IBM/plex

In the `"actions"` section add these keybindings:

```json
{ "command": { "action": "newTab"  }, "keys": "ctrl+shift+t" },
{ "command": { "action": "nextTab" }, "keys": "ctrl+shift+right" },
{ "command": { "action": "prevTab" }, "keys": "ctrl+shift+left" }
```

And **REMOVE** the `ctrl+v` binding, if you want to use `ctrl+v` in vim (visual
line selection.)

This gives you a sort of "tmux" for powershell using tabs.

Restart the terminal.

#### Setting up Vim

If you don't use vim, just add an alias for your favorite editor in your
powershell `$profile`, and set `$env:EDITOR` so that git can open it for commit
messages etc.. I will explain how to do this below.

If you are using neovim, make some adjustments to the following instructions,
and do the following:

```powershell
mkdir ~/.vim
ni -itemtype symboliclink ~/AppData/Local/nvim -target ~/.vim
ni -itemtype symboliclink ~/.vim/init.vim      -target ~/.vimrc
```

You can edit your powershell profile with `vim $profile`, and reload it with `.
$profile`.

Add the following to your `$profile`:

```powershell
if ($env:TERM) { ri env:TERM }
$env:EDITOR = resolve-path ~/bin/vim.bat
```

In `~/bin/vim.bat` put the following:

```bat
@echo off
set TERM=
c:/windows/vim.bat %*
```

This is needed for git to work correctly with native vim.

Some suggestions for your `~/.vimrc`:

```vim
set encoding=utf8
set langmenu=en_US.UTF-8
let g:is_bash=1
set formatlistpat=^\\s*\\%([-*][\ \\t]\\\|\\d+[\\]:.)}\\t\ ]\\)\\s*
set ruler bg=dark nohlsearch bs=2 ai fo+=n modeline belloff=all
set fileformats=unix,dos

" Add vcpkg includes to include search path to get completions for C++.
let g:home = fnamemodify('~', ':p')

if isdirectory(g:home . 'source/repos/vcpkg/installed/x64-windows-static/include')
  let &path .= ',' . g:home . 'source/repos/vcpkg/installed/x64-windows-static/include'
endif

if isdirectory(g:home . 'source/repos/vcpkg/installed/x64-windows-static/include/SDL2')
  let &path .= ',' . g:home . 'source/repos/vcpkg/installed/x64-windows-static/include/SDL2'
endif

set termguicolors
au ColorScheme * hi Normal guibg=#000000

if (has('win32') || has('gui_win32')) && executable('pwsh')
    set shell=pwsh
    set shellcmdflag=\ -ExecutionPolicy\ RemoteSigned\ -NoProfile\ -Nologo\ -NonInteractive\ -Command
endif

filetype plugin indent on
syntax enable

au BufRead COMMIT_EDITMSG,*.md setlocal spell
```

I use this color scheme, which is a fork of Apprentice for black backgrounds:

https://github.com/rkitover/Apprentice

You can add with Plug or pathogen or whatever you prefer.

All of this works with neovim.

#### Set up PowerShell Profile

Now add some useful things to your powershell profile, I will present some of
mine below:

Run:

```powershell
vim $profile
```

or

```powershell
notepad $profile
```

If you use my posh-git prompt, you'll need the git version of posh-git:

```powershell
mkdir ~/source/repos
cd ~/source/repos
git clone https://github.com/dahlbyk/posh-git
```

Here is a profile to get you started, it has a few examples of functions and
aliases which you will invariably write for yourself:

```powershell
chcp 65001 > $null

set-executionpolicy -scope currentuser remotesigned

set-culture en-US

$terminal_settings = resolve-path ~/AppData/Local/Packages/Microsoft.WindowsTerminal_*/LocalState/settings.json

if ($env:TERM) { ri env:TERM }
$env:EDITOR = resolve-path ~/bin/vim.bat

function megs {
    gci -rec $args | select mode, lastwritetime, @{name="MegaBytes"; expression = { [math]::round($_.length / 1MB, 2) }}, name
}

function cmconf {
    grep -E --color 'CMAKE_BUILD_TYPE|VCPKG_TARGET_TRIPLET|UPSTREAM_RELEASE' CMakeCache.txt
}

function pgrep {
    get-ciminstance win32_process -filter "name like '%$($args[0])%' OR commandline like '%$($args[0])%'" | select processid, name, commandline
}

function pkill {
    pgrep $args | %{ stop-process $_.processid }
}

function taskslog {
    get-winevent 'Microsoft-Windows-TaskScheduler/Operational' 
}

function ltr { $input | sort lastwritetime }

function ntop { ntop.exe -s 'CPU%' $args }

function head {
    $lines = if ($args.length -and $args[0] -match '^-(.+)') { $null,$args = $args; $matches[1] } else { 10 }
    
    if (!$args.length) {
        $input | select -first $lines
    }
    else {
        gc $args | select -first $lines
    }
}

set-alias -name which   -val get-command
set-alias -name notepad -val '/program files/notepad++/notepad++'

if (test-path alias:diff) { remove-item -force alias:diff }

# Load VS env only once.
foreach ($vs_type in 'buildtools','community') {
    $vs_path="/program files (x86)/microsoft visual studio/2019/${vs_type}/vc/auxiliary/build"

    if (test-path $vs_path) {
        break
    }
    else {
        $vs_path=$null
    }
}

if ($vs_path -and -not $env:VSCMD_VER) {
    pushd $vs_path
    cmd /c 'vcvars64.bat & set' | where { $_ -match '=' } | %{
        $var,$val = $_.split('=')
        set-item -force "env:$var" -value $val
    }
    popd
}

# Chocolatey profile
$chocolatey_profile = "$env:chocolateyinstall\helpers\chocolateyprofile.psm1"

if (test-path $chocolatey_profile) { import-module $chocolatey_profile }

import-module ~/source/repos/posh-git/src/posh-git.psd1

function global:PromptWriteErrorInfo() {
    if ($global:gitpromptvalues.dollarquestion) {
        ([char]27) + '[0;32mv' + ([char]27) + '[0m'
    }
    else {
        ([char]27) + '[0;31mx' + ([char]27) + '[0m'
    }
}

$gitpromptsettings.defaultpromptabbreviatehomedirectory      = $true

$gitpromptsettings.defaultpromptprefix.text                  = '$(PromptWriteErrorInfo) '

$gitpromptsettings.defaultpromptwritestatusfirst             = $false
$gitpromptsettings.defaultpromptbeforesuffix.text            = "`n$env:USERNAME@$($env:COMPUTERNAME.ToLower()) "
$gitpromptsettings.defaultpromptbeforesuffix.foregroundcolor = 0x87CEFA
$gitpromptsettings.defaultpromptsuffix.foregroundcolor       = 0xDC143C

import-module psreadline

set-psreadlineoption     -editmode emacs
set-psreadlinekeyhandler -key tab       -function tabcompletenext
set-psreadlinekeyhandler -key uparrow   -function historysearchbackward
set-psreadlinekeyhandler -key downarrow -function historysearchforward
```

This profile works for "Windows PowerShell", the powershell you launch from the
`Win+X` menu as well. But the profile is in a different file, so you will need
to copy it there too:

```powershell
mkdir ~/Documents/WindowsPowerShell
cpi ~/Documents/PowerShell/Microsoft.Powershell_profile.ps1 ~/Documents/WindowsPowerShell
```

#### Setting up gpg

Make this symlink:

```powershell
sl ~
mkdir .gnupg
ni -itemtype symboliclink ~/AppData/Roaming/gnupg -target $(resolve-path ~/.gnupg)
```

Then you can copy your `.gnupg` over, without the socket files.

To configure git to use it, do the following:

```powershell
git config --global commit.gpgsign true
git config --global gpg.program 'C:\Program Files (x86)\GnuPG\bin\gpg.exe'
```

#### Setting up sshd

Edit `\ProgramData\ssh\sshd_config` and remove or comment out this section:

```
Match Group administrators
       AuthorizedKeysFile __PROGRAMDATA__/ssh/administrators_authorized_keys
```

Then run:

```powershell
restart-service sshd
```

#### PowerShell Usage Notes

PowerShell is very different from unix shells, in both usage and programming.

This section won't teach you PowerShell, but it will give you enough
information to use it as a shell and a springboard for further exploration.

You can get a list of aliases with `alias` and lookup specific aliases with e.g.
`alias ri`. It allows globs, e.g. to see aliases starting with `s` do `alias
s*`.

You can get help text for any cmdlet via its long name or alias with `help`. To
use `less` instead of the default pager, do e.g.: `help gci | less`.

For the `git` man pages, do `git help <command>` to open the man page in your
browser, e.g. `git help config`.

I suggest using the short forms of PowerShell aliases instead of the POSIX
aliases, this forces your brain into PowerShell mode so you will mix things up
less often, with the exception of a couple of things like `mkdir` and the alias
above for `which`.

Here is a few:

| PowerShell alias | Full cmdlet + Params          | POSIX command     |
|------------------|-------------------------------|-------------------|
| sl               | Set-Location                  | cd                |
| gci              | Get-ChildItem                 | ls                |
| gi               | Get-Item                      | ls -d             |
| cpi              | Copy-Item                     | cp -r             |
| ri               | Remove-Item                   | rm                |
| ri -for          | Remove-Item -Force            | rm -f             |
| ri -rec -for     | Remove-Item -Force -Recurse   | rm -rf            |
| gc               | Get-Content                   | cat               |
| mi               | Move-Item                     | mv                |
| mkdir            | New-Item -ItemType Directory  | mkdir             |
| which (custom)   | Get-Command                   | command -v, which |
| gci -rec         | Get-ChildItem -Recurse        | find              |
| ni               | New-Item                      | touch <new-file>  |
| sort             | Sort-Object                   | sort              |
| sort -u          | Sort-Object -Unique           | sort -u           |

This will get you around and doing stuff, the usage is slightly different
however.

For one thing commands like `cpi` (`Copy-Item`) take a list of files differently
from POSIX, they must be a PowerShell list, which means separated by commas. For
example, to copy `file1` and `file2` to `dest-dir`, you would do:

```powershell
cpi file1,file2 dest-dir
```

To remove `file1` and `file2` you would do:

```powershell
ri file1,file2
```

You can list multiple globs in these lists as well as files and directories
etc., for example:

```powershell
ri .*.un~,.*.sw?
```

The commands `grep`, `sed`, `awk`, `rg`, `diff`, `patch`, `less`, `zip`,
`unzip`, `ssh`, `vim`, `nvim` (neovim) are the same as in Linux and were
installed in the list of packages installed from Chocolatey above.

The commands `curl` and `tar` are now standard Windows commands.

For an `htop` replacement, use `ntop` (installed in the list of Chocolatey
packages above.) with my wrapper function in the sample `$profile`.

Redirection for files and commands works like in POSIX on a basic level, that
is, you can expect `<`, `>` and `|` to redirect files and commands like you
would expect on a POSIX shell. `/dev/null` is `$null`, so the equivalent of

```bash
cmd >/dev/null 2>&1
```

would be:

```powershell
cmd *> $null
```

For `ls -ltr` use:

```powershell
gci | sort lastwritetime
```

Or the alias in my profile:

```powershell
gci | ltr
```

Parameters can be completed with `tab`, so in the case above you could write
`lastw<tab>`.

To make a symbolic link, do:

```powershell
ni -itemtype symboliclink name-of-link -target path-to-source
```

again the parameters `-ItemType` and `SymbolicLink` can be `tab` completed.

For a `find` replacement, use the `-Recurse` flag to `gci`, e.g.:

```powershell
gci -rec *.cpp
```

PowerShell supports an amazing new system called the "object pipeline", what
this means is that you can pass objects around via pipelines and inspect their
properties, call methods on them, etc..

Here is an example of using the object pipeline to delete all vim undo files:

```powershell
gci -rec .*.un~ | ri
```

it's that simple, `ri` notices that the input objects are files, and removes
them.

You can access the piped-in input in your own functions as the special `$input`
variable, like in the `head` example in the profile above.

Here is a more typical example:

```powershell
get-process | ?{ $_.name -notmatch 'svchost' } | %{ $_.name } | sort -uniq
```

here `?{ ... }` is like filter/grep block and `%{ ... }` is like apply/map.

In PowerShell, the backtick `` ` `` is the escape character, and you can use it
at the end of a line, escaping the line end as a line continuation character. In
regular expressions, the backslash `\` is the escape character, like everywhere
else.

Here are a couple more example of PowerShell one-liners:

```powershell
# Name and command mapping for aliases starting with 'se'.
alias se* | select name, resolvedcommand

# Create new empty files foo1 .. foo7.
1..7 | %{ ni "foo$_" }

# Find the import libraries in the Windows SDK with symbol names matching
# 'MessageBox'.
gci '/program files (x86)/windows kits/10/lib/10.*/um/x64/*.lib' | `
  %{ $_.name; dumpbin -headers $_ | grep MessageBox }
```

#### Miscellaneous

To get transparency in Microsoft terminal, use this AutoHotkey script:

```autohotkey
#NoEnv
SendMode Input
SetWorkingDir %A_ScriptDir%

; Toggle window transparency.
#^Esc::
WinGet, TransLevel, Transparent, A
If (TransLevel = 255) {
    WinSet, Transparent, 205, A
} Else {
    WinSet, Transparent, 255, A
}
return
```

This will toggle transparency in a window when you press `Ctrl+Win+Esc`, you
have to press it twice the first time.

Thanks to @munael for this tip.

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
