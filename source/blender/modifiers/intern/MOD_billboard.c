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

#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"

#include "MOD_util.h"

static void initData(ModifierData *UNUSED(md))
{
  //BillboardModifierData *bmd = (BillboardModifierData *)md;
}

static DerivedMesh *applyModifier(ModifierData *UNUSED(md),
                                  Object *UNUSED(ob),
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
  //BillboardModifierData *bmd = (BillboardModifierData *)md;
  return dm;
}

ModifierTypeInfo modifierType_Billboard = {
  /* name */              "Billboard",
  /* structName */        "BillboardModifierData",
  /* structSize */        sizeof(BillboardModifierData),
  /* type */              eModifierTypeType_Constructive,
  /* flags */             eModifierTypeFlag_AcceptsMesh |
                          eModifierTypeFlag_SupportsEditmode,

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
  /* updateDepgraph */    NULL,
  /* dependsOnTime */     NULL,
  /* dependsOnNormals */  NULL,
  /* foreachObjectLink */ NULL,
  /* foreachIDLink */     NULL,
  /* foreachTexLink */    NULL,
};

