Blender Billboard Modifier
==========================

This modifier is currently under development.
This repository should be considered unstable.

This modifier will transform 3-D geometry into 2-D billboards (planes)
orientated to face a reference object.
There are a few possible uses which come to mind at this stage in
development: as an alternative to wireframe/skin for creating
geometry that can be rendered from edge-only meshes; or as a method
for simplifying complex geometry to a single plane-texture pair.

Development Documentation
=========================

Throughout development, I will be documenting my progress incrementally
in this README file so that this project might be useful for anyone
interested in attempting to implement their own Blender modifier
(or, at the very least, remind me of how to implement a modifier next
time I forget).

Downloading Blender
-------------------

From http://wiki.blender.org/index.php/Dev:Doc/Tools/Git

```bash
git clone git://git.blender.org/blender.git
cd blender
git submodule update --init --recursive
git submodule foreach git checkout master
git submodule foreach git pull --rebase origin master
```

Setting up Billboard repository
-------------------------------

I wanted to keep my modifier development branch separate from my
main blender branch.
I also wanted the new branch to only include
an initial snapshot of the blender development branch and squashed
merges in order to keep things as clean as possible.

The new branch was created as an orphan (initialised with the
contents of master).

```bash
git checkout master
git checkout --orphan billboard
git commit -m "Blender merge 0"
```

The new branch was then linked to the remote GitHub repository
and the contents pushed.

```bash
git remote add github https://github.com/ilent2/Blender-Billboard-Modifier.git
git fetch github
git push [-f] github billboard:master
```

Finally, to merge in new changes from the Blender master branch
I would use the commands:

```bash
git merge --squash master
git commit -m "Blender merge <N>"
```

