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

Adding place holders for Billboard modifier
-------------------------------------------

The next task in developing this modifier was to create place holders
for the modifier implementation.
My initial guide was the slightly outdated tutorial at
http://wiki.blender.org/index.php/User:Enjalot/Creating_a_custom_modifier .
The easiest approach is to find a modifier that is similar to the
modifier we intend to develop and running grep on the Blender source
tree with terms related to the modifier.

The changes needed to implement a new operator are divided into
four categories:

-  modifier implementation
-  makesdna entries (modifier data structure)
-  makesrna entries (RNA structure, UI text, description, icon name, ...)
-  Declaration and drawing of an icon
-  Python bl_ui user interface code

A full summary of changes can be found in the commit
ilent2@f182cdc910c477079a434cd31186337cee29a147

The basic idea of this modifier is to create a plane orientated
towards a reference object for each edge in a mesh.
The length of the plane is controlled by the edge length and
the width by a thickness parameter in the modifier UI.
At a later stage it might be possible to handle faces, vertices
or even curves.

This initial prototype requires the addition of two UI properties.
We declare these new properties inside the BillboardData structure
in DNA_modifier_types.h.

```c++
/* Billboard Modifier */
typedef struct BillboardModifierData {
  ModifierData modifier;

  // The two new properties offset and object
  float offset, pad;
  struct Object *object;

} BillboardModifierData;
```

Notice the addition of the padding variable.
Blender DNA must be aligned, there are a couple of pages which
describe this, a quick Google returned this page
http://wiki.blender.org/index.php/Dev:Source/Data_Structures/DNAStructs .

The new RNA entries for the offset and object where copied from
the Wireframe and Mirror modifier RNA declarations respectively.
The Mirror modifier uses a reference object in roughly the same
way as our modifier.
The offset property is just a float so there is not too much
to declaring this property.

In our main modifier source file, we have implemented a rough
version of the modifier (probably not very efficient nor very
clean and orderly).
The other addition to the main source file are the updateDepgraph
and foreachObjectLink functions.
These two functions where pretty much copied verbatim from the
mirror modifier source code.
The first function allows the modifier to respond to changes to
the reference object in real time.
The second function seems to be related to loading and linking.

ilent2@4e7da3523c7d1e8be12597a1fbcd4360ed802b90

