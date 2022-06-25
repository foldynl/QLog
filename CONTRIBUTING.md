## How Can I Contribute?

### Reporting Bugs
*Please* contact me via the repository's Discussion page or email (you can find it 
[QRZ](https://www.qrz.com/db/OK1MLG) or [HamQTH](https://www.hamqth.com/ok1mlg) ) 

If you run into an issue, please search [Issue tracking](https://github.com/foldynl/QLog/issues) 
*first* to ensure the issue hasn't been reported before. Open a new issue only if you haven't 
found anything similar to your issue.

#### When opening a new issue, please include the following information at the top of the issue:
* What version you are using.
* Describe the issue you are experiencing.
* Describe how to reproduce the issue.
* Including any warning/errors/backtraces - see debug details in 
QLog [Wiki](https://github.com/foldynl/QLog/wiki/Debug-Log-Level).

In general, the more detail you share about a problem the quicker a
developer can resolve it. For example, providing a simple test case is always
exceptionally helpful.

Be prepared to work with the developers investigating your issue. Your
assistance is crucial in providing a quick solution. They may ask for
information like:

* Your non-sensitive information - ADIF export, log file etc.
* Your hardware configuration
* Your OS, distribution etc.
* Your version of libraries (especial Qt, Hamlib)

### Pull Requests

#### General

* All pull requests must be based on the current `master` branch 
and should apply without conflicts.
* Please attempt to limit pull requests to a single commit which resolves
one specific issue.
* Make sure your commit messages are in the correct format. See the
[Commit Message Formats](#commit-message-formats) section for more information.
* When updating a pull request squash multiple commits by performing a
[rebase](https://git-scm.com/docs/git-rebase) (squash).
* For large pull requests consider structuring your changes as a stack of
logically independent patches which build on each other.  This makes large
changes easier to review and approve which speeds up the merging process.
* Try to keep pull requests simple. Simple code with comments is much easier
to review and approve.
* If you have an idea you'd like to discuss or which requires additional testing, consider opening it as a draft pull request.
Once everything is in good shape and the details have been worked out you can remove its draft status.
Any required reviews can then be finalized and the pull request merged.

### Testing
If you're in a position to run the latest code
consider helping us by reporting any functional problems, performance
regressions or other suspected issues. By running the latest code to a wide
range of realistic workloads, configurations and architectures we're better
able quickly identify and resolve potential issues.

## Style Guides

### Repository Structure

**Branch Names:**
- Latest Public Release branch: `master`
- Upcoming release branch: `testing_$VERSION`

### Coding Conventions

* Conditional 
```
if ( cond )
{
   command;
}
````
* Use spaces around operators
    * `count + 1` instead of `count+1`
* Use spaces after commas (unless separated by newlines)
* Keep in mind that QLog can run under Linux, Windows and MacOS - do not use platform-depend code

#### Modules
If possible, one class one file. Each mode (except models) 
has to include `debug.h` and define 
```
MODULE_IDENTIFICATION("qlog.ui.mainwindow");
```

Each function (except models) has to contain 
```
FCT_IDENTIFICATION;
```

### Commit Message Formats
#### New Changes
Commit messages for new changes must meet the following guidelines:
* In 72 characters or less, provide a summary of the change as the
first line in the commit message.
* A body which provides a description of the change. If necessary,
please summarize important information such as why the proposed
approach was chosen or a brief description of the bug you are resolving.
Each line of the body must be 72 characters or less.
* Provides a subject line in the format of
`Module_Name: Change description` where `Module_name` defines a part of source code where the change was made or new feature name (short name).

```
Module_Name: This line is a brief summary of your change

Please provide at least a couple sentences describing the
change. If necessary, please summarize decisions such as
why the proposed approach was chosen or what bug you are
attempting to solve.
```

#### Bug Fixes
If you are submitting a fix
the commit message should meet the following guidelines:
* Provides a subject line in the format of
`Fixed #ddd: Fix name...` where `ddd` represents
each [Issue](https://github.com/foldynl/QLog/issues) .

Bugfixes not listed in the Github Issue are introduced by the same way as Improvements (Module Name etc).
