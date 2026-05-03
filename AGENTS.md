Addivox is a software synth, which can be built both as a standalone app and as a plugin for a DAW. Whenever possible, it should follow industry conventions for tools such as these.

Don't make any changes to the git repo. So no `git add`, `git commit`, `git push`, etc. I'll take care of committing, pushing, etc. You're welcome to run commands that don't change anything like `git status` or `git log`, if you want.

The `iPlug2/` directory is a submodule, and you are not allowed to edit any files in there.

For code style, you can look at the `.clang-format` file. I prefer long lines (160 characters) and table-like alignment when possible. You can run `clang-format` on new code that you write, but don't run `clang-format` on the entire repo, since I have some custom formatting that I want to keep.

I'm developing on a Mac, so you can test Xcode builds yourself. But Addivox should also build on Windows, so do you best to ensure Windows compatibility, even if you can't directly test it.
