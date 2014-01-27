/* *********************** BEGIN LICENSE BLOCK *******************************\
 *
 * MOD_billboard.c - Modifier for generating billboards from 3-D geometry.
 * Copyright (C) 2013 Isaac Lenton (aka ilent2)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
\* *********************** END LICENSE BLOCK *********************************/

/** \file blender/modifiers/intern/MOD_billboard.c
*  \ingroup modifiers
*/

#include "MEM_guardedalloc.h"
#include "depsgraph_private.h"

#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"

#include "BLI_math.h"

#include "MOD_util.h"

#include "bmesh.h"

static DerivedMesh *BillboardModifier_do(BillboardModifierData *bmd,
                                         Object *ob,
                                         DerivedMesh *dm)
{
  DerivedMesh *result;
  BMesh *bm;

  BMIter iter;
  BMFace *f, *f_next;
  BMEdge *e_src, *edges[4];
  BMVert *verts[4];
  float screen_axis[3] = {1.0, 0.0, 0.0};
  float edge_axis[3], shift_axis[3];

  if (bmd->object) {
    sub_v3_v3v3(screen_axis, ob->loc, bmd->object->loc);
    normalize_v3(screen_axis);
  }

  bm = DM_to_bmesh(dm, true);

  BM_ITER_MESH_MUTABLE (f, f_next, &iter, bm, BM_FACES_OF_MESH) {
    BM_face_kill(bm, f);
  }

  BM_ITER_MESH (verts[0], &iter, bm, BM_VERTS_OF_MESH) {
    BM_elem_flag_enable(verts[0], BM_ELEM_TAG);
  }

  BM_ITER_MESH (e_src, &iter, bm, BM_EDGES_OF_MESH) {
    if (!BM_elem_flag_test(e_src->v1, BM_ELEM_TAG)) break;

    verts[0] = BM_vert_create(bm, e_src->v1->co, e_src->v1, BM_CREATE_NOP);
    verts[1] = BM_vert_create(bm, e_src->v2->co, e_src->v2, BM_CREATE_NOP);
    verts[2] = BM_vert_create(bm, e_src->v2->co, e_src->v2, BM_CREATE_NOP);
    verts[3] = BM_vert_create(bm, e_src->v1->co, e_src->v1, BM_CREATE_NOP);

    sub_v3_v3v3(edge_axis, e_src->v1->co, e_src->v2->co);
    normalize_v3(edge_axis);
    cross_v3_v3v3(shift_axis, screen_axis, edge_axis);

    madd_v3_v3fl(verts[0]->co, shift_axis, -0.5*bmd->offset);
    madd_v3_v3fl(verts[1]->co, shift_axis, -0.5*bmd->offset);
    madd_v3_v3fl(verts[2]->co, shift_axis, 0.5*bmd->offset);
    madd_v3_v3fl(verts[3]->co, shift_axis, 0.5*bmd->offset);

    if ((bmd->flag & MOD_BILLBOARD_FLATTEN) != 0) {
      madd_v3_v3fl(verts[0]->co, screen_axis,
          -dot_v3v3(screen_axis, verts[0]->co));
      madd_v3_v3fl(verts[1]->co, screen_axis,
          -dot_v3v3(screen_axis, verts[1]->co));
      madd_v3_v3fl(verts[2]->co, screen_axis,
          -dot_v3v3(screen_axis, verts[2]->co));
      madd_v3_v3fl(verts[3]->co, screen_axis,
          -dot_v3v3(screen_axis, verts[3]->co));
    }

    BM_elem_flag_disable(verts[0], BM_ELEM_TAG);
    BM_elem_flag_disable(verts[1], BM_ELEM_TAG);
    BM_elem_flag_disable(verts[2], BM_ELEM_TAG);
    BM_elem_flag_disable(verts[3], BM_ELEM_TAG);

    edges[0] = BM_edge_create(bm, verts[0], verts[1], e_src, BM_CREATE_NOP);
    edges[1] = BM_edge_create(bm, verts[1], verts[2], e_src, BM_CREATE_NOP);
    edges[2] = BM_edge_create(bm, verts[2], verts[3], e_src, BM_CREATE_NOP);
    edges[3] = BM_edge_create(bm, verts[3], verts[0], e_src, BM_CREATE_NOP);

    BM_face_create(bm, verts, edges, 4, NULL, BM_CREATE_NOP);
  }

  BM_ITER_MESH_MUTABLE (verts[0], verts[1], &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(verts[0], BM_ELEM_TAG)) {
      BM_vert_kill(bm, verts[0]);
    }
  }

  result = CDDM_from_bmesh(bm, true);
  BM_mesh_free(bm);

  return result;
}

static void initData(ModifierData *md)
{
  BillboardModifierData *bmd = (BillboardModifierData *)md;
  bmd->offset = 0.02f;
}

static DerivedMesh *applyModifier(ModifierData *md,
                                  Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
  return BillboardModifier_do((BillboardModifierData *)md, ob, dm);
}

static void foreachObjectLink(ModifierData *md, Object *ob,
    void (*walk)(void *userData, Object *ob, Object ** obpointer),
    void *userData)
{
  BillboardModifierData *bmd = (BillboardModifierData *)md;
  walk(userData, ob, &bmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
  BillboardModifierData *bmd = (BillboardModifierData *)md;
  if (bmd->object) {
    DagNode *latNode = dag_get_node(forest, bmd->object);
    dag_add_relation(forest, latNode, obNode,
        DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Billboard Modifier");
  }
}

ModifierTypeInfo modifierType_Billboard = {
  /* name */              "Billboard",
  /* structName */        "BillboardModifierData",
  /* structSize */        sizeof(BillboardModifierData),
  /* type */              eModifierTypeType_Constructive,
  /* flags */             eModifierTypeFlag_AcceptsMesh |
                          eModifierTypeFlag_SupportsEditmode |
                          eModifierTypeFlag_EnableInEditmode,

  /* copyData */          NULL,
  /* deformVerts */       NULL,
  /* deformMatrices */    NULL,
  /* deformVertsEM */     NULL,
  /* deformMatricesEM */  NULL,
  /* applyModifier */     applyModifier,
  /* applyModifierEM */   NULL,
  /* initData */          initData,
  /* requiredDataMask */  NULL,
  /* freeData */          NULL,
  /* isDisabled */        NULL,
  /* updateDepgraph */    updateDepgraph,
  /* dependsOnTime */     NULL,
  /* dependsOnNormals */  NULL,
  /* foreachObjectLink */ foreachObjectLink,
  /* foreachIDLink */     NULL,
  /* foreachTexLink */    NULL,
};

