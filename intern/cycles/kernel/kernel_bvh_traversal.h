/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation,
 * and code copyright 2009-2012 Intel Corporation
 *
 * Modifications Copyright 2011-2013, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a template BVH traversal function, where various features can be
 * enabled/disabled. This way we can compile optimized versions for each case
 * without new features slowing things down.
 *
 * BVH_INSTANCING: object instancing
 * BVH_HAIR: hair curve rendering
 * BVH_HAIR_MINIMUM_WIDTH: hair curve rendering with minimum width
 * BVH_MOTION: motion blur rendering
 *
 */

#define FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

ccl_device bool BVH_FUNCTION_NAME
(KernelGlobals *kg, const Ray *ray, Intersection *isect, const uint visibility
#if FEATURE(BVH_HAIR_MINIMUM_WIDTH)
, uint *lcg_state, float difl, float extmax
#endif
)
{
	/* todo:
	 * - test if pushing distance on the stack helps (for non shadow rays)
	 * - separate version for shadow rays
	 * - likely and unlikely for if() statements
	 * - SSE for hair
	 * - test restrict attribute for pointers
	 */
	
	/* traversal stack in CUDA thread-local memory */
	int traversalStack[BVH_STACK_SIZE];
	traversalStack[0] = ENTRYPOINT_SENTINEL;

	/* traversal variables in registers */
	int stackPtr = 0;
	int nodeAddr = kernel_data.bvh.root;

	/* ray parameters in registers */
	const float tmax = ray->t;
	float3 P = ray->P;
	float3 idir = bvh_inverse_direction(ray->D);
	int object = ~0;

#if FEATURE(BVH_MOTION)
	Transform ob_tfm;
#endif

	isect->t = tmax;
	isect->object = ~0;
	isect->prim = ~0;
	isect->u = 0.0f;
	isect->v = 0.0f;

#if defined(__KERNEL_SSE2__) && !FEATURE(BVH_HAIR_MINIMUM_WIDTH)
	const shuffle_swap_t shuf_identity = shuffle_swap_identity();
	const shuffle_swap_t shuf_swap = shuffle_swap_swap();
	
	const __m128 pn = _mm_castsi128_ps(_mm_set_epi32(0x80000000, 0x80000000, 0, 0));
	__m128 Psplat[3], idirsplat[3];

	Psplat[0] = _mm_set_ps1(P.x);
	Psplat[1] = _mm_set_ps1(P.y);
	Psplat[2] = _mm_set_ps1(P.z);

	idirsplat[0] = _mm_xor_ps(_mm_set_ps1(idir.x), pn);
	idirsplat[1] = _mm_xor_ps(_mm_set_ps1(idir.y), pn);
	idirsplat[2] = _mm_xor_ps(_mm_set_ps1(idir.z), pn);

	__m128 tsplat = _mm_set_ps(-isect->t, -isect->t, 0.0f, 0.0f);

	shuffle_swap_t shufflex = (idir.x >= 0)? shuf_identity: shuf_swap;
	shuffle_swap_t shuffley = (idir.y >= 0)? shuf_identity: shuf_swap;
	shuffle_swap_t shufflez = (idir.z >= 0)? shuf_identity: shuf_swap;
#endif

	/* traversal loop */
	do {
		do
		{
			/* traverse internal nodes */
			while(nodeAddr >= 0 && nodeAddr != ENTRYPOINT_SENTINEL)
			{
				bool traverseChild0, traverseChild1;
				int nodeAddrChild1;

#if !defined(__KERNEL_SSE2__) || FEATURE(BVH_HAIR_MINIMUM_WIDTH)
				/* Intersect two child bounding boxes, non-SSE version */
				float t = isect->t;

				/* fetch node data */
				float4 node0 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+0);
				float4 node1 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+1);
				float4 node2 = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+2);
				float4 cnodes = kernel_tex_fetch(__bvh_nodes, nodeAddr*BVH_NODE_SIZE+3);

				/* intersect ray against child nodes */
				NO_EXTENDED_PRECISION float c0lox = (node0.x - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c0hix = (node0.z - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c0loy = (node1.x - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c0hiy = (node1.z - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c0loz = (node2.x - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c0hiz = (node2.z - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c0min = max4(min(c0lox, c0hix), min(c0loy, c0hiy), min(c0loz, c0hiz), 0.0f);
				NO_EXTENDED_PRECISION float c0max = min4(max(c0lox, c0hix), max(c0loy, c0hiy), max(c0loz, c0hiz), t);

				NO_EXTENDED_PRECISION float c1lox = (node0.y - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c1hix = (node0.w - P.x) * idir.x;
				NO_EXTENDED_PRECISION float c1loy = (node1.y - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c1hiy = (node1.w - P.y) * idir.y;
				NO_EXTENDED_PRECISION float c1loz = (node2.y - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c1hiz = (node2.w - P.z) * idir.z;
				NO_EXTENDED_PRECISION float c1min = max4(min(c1lox, c1hix), min(c1loy, c1hiy), min(c1loz, c1hiz), 0.0f);
				NO_EXTENDED_PRECISION float c1max = min4(max(c1lox, c1hix), max(c1loy, c1hiy), max(c1loz, c1hiz), t);

#if FEATURE(BVH_HAIR_MINIMUM_WIDTH)
				if(difl != 0.0f) {
					float hdiff = 1.0f + difl;
					float ldiff = 1.0f - difl;
					if(__float_as_int(cnodes.z) & PATH_RAY_CURVE) {
						c0min = max(ldiff * c0min, c0min - extmax);
						c0max = min(hdiff * c0max, c0max + extmax);
					}
					if(__float_as_int(cnodes.w) & PATH_RAY_CURVE) {
						c1min = max(ldiff * c1min, c1min - extmax);
						c1max = min(hdiff * c1max, c1max + extmax);
					}
				}
#endif

				/* decide which nodes to traverse next */
#ifdef __VISIBILITY_FLAG__
				/* this visibility test gives a 5% performance hit, how to solve? */
				traverseChild0 = (c0max >= c0min) && (__float_as_uint(cnodes.z) & visibility);
				traverseChild1 = (c1max >= c1min) && (__float_as_uint(cnodes.w) & visibility);
#else
				traverseChild0 = (c0max >= c0min);
				traverseChild1 = (c1max >= c1min);
#endif

#else // __KERNEL_SSE2__
				/* Intersect two child bounding boxes, SSE3 version adapted from Embree */

				/* fetch node data */
				__m128 *bvh_nodes = (__m128*)kg->__bvh_nodes.data + nodeAddr*BVH_NODE_SIZE;
				float4 cnodes = ((float4*)bvh_nodes)[3];

				/* intersect ray against child nodes */
				const __m128 tminmaxx = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[0], shufflex), Psplat[0]), idirsplat[0]);
				const __m128 tminmaxy = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[1], shuffley), Psplat[1]), idirsplat[1]);
				const __m128 tminmaxz = _mm_mul_ps(_mm_sub_ps(shuffle_swap(bvh_nodes[2], shufflez), Psplat[2]), idirsplat[2]);

				const __m128 tminmax = _mm_xor_ps(_mm_max_ps(_mm_max_ps(tminmaxx, tminmaxy), _mm_max_ps(tminmaxz, tsplat)), pn);
				const __m128 lrhit = _mm_cmple_ps(tminmax, shuffle_swap(tminmax, shuf_swap));

				/* decide which nodes to traverse next */
#ifdef __VISIBILITY_FLAG__
				/* this visibility test gives a 5% performance hit, how to solve? */
				traverseChild0 = (_mm_movemask_ps(lrhit) & 1) && (__float_as_uint(cnodes.z) & visibility);
				traverseChild1 = (_mm_movemask_ps(lrhit) & 2) && (__float_as_uint(cnodes.w) & visibility);
#else
				traverseChild0 = (_mm_movemask_ps(lrhit) & 1);
				traverseChild1 = (_mm_movemask_ps(lrhit) & 2);
#endif
#endif // __KERNEL_SSE2__

				nodeAddr = __float_as_int(cnodes.x);
				nodeAddrChild1 = __float_as_int(cnodes.y);

				if(traverseChild0 && traverseChild1) {
					/* both children were intersected, push the farther one */
#if !defined(__KERNEL_SSE2__) || FEATURE(BVH_HAIR_MINIMUM_WIDTH)
					bool closestChild1 = (c1min < c0min);
#else
					union { __m128 m128; float v[4]; } uminmax;
					uminmax.m128 = tminmax;
					bool closestChild1 = uminmax.v[1] < uminmax.v[0];
#endif

					if(closestChild1) {
						int tmp = nodeAddr;
						nodeAddr = nodeAddrChild1;
						nodeAddrChild1 = tmp;
					}

					++stackPtr;
					traversalStack[stackPtr] = nodeAddrChild1;
				}
				else {
					/* one child was intersected */
					if(traverseChild1) {
						nodeAddr = nodeAddrChild1;
					}
					else if(!traverseChild0) {
						/* neither child was intersected */
						nodeAddr = traversalStack[stackPtr];
						--stackPtr;
					}
				}
			}

			/* if node is leaf, fetch triangle list */
			if(nodeAddr < 0) {
				float4 leaf = kernel_tex_fetch(__bvh_nodes, (-nodeAddr-1)*BVH_NODE_SIZE+(BVH_NODE_SIZE-1));
				int primAddr = __float_as_int(leaf.x);

#if FEATURE(BVH_INSTANCING)
				if(primAddr >= 0) {
#endif
					int primAddr2 = __float_as_int(leaf.y);

					/* pop */
					nodeAddr = traversalStack[stackPtr];
					--stackPtr;

					/* primitive intersection */
					while(primAddr < primAddr2) {
						bool hit;

						/* intersect ray against primitive */
#if FEATURE(BVH_HAIR)
						uint segment = kernel_tex_fetch(__prim_segment, primAddr);
						if(segment != ~0) {

							if(kernel_data.curve.curveflags & CURVE_KN_INTERPOLATE) 
#if FEATURE(BVH_HAIR_MINIMUM_WIDTH)
								hit = bvh_cardinal_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment, lcg_state, difl, extmax);
							else
								hit = bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment, lcg_state, difl, extmax);
#else
								hit = bvh_cardinal_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
							else
								hit = bvh_curve_intersect(kg, isect, P, idir, visibility, object, primAddr, segment);
#endif
						}
						else
#endif
							hit = bvh_triangle_intersect(kg, isect, P, idir, visibility, object, primAddr);

						/* shadow ray early termination */
#if defined(__KERNEL_SSE2__) && !FEATURE(BVH_HAIR_MINIMUM_WIDTH)
						if(hit) {
							if(visibility == PATH_RAY_SHADOW_OPAQUE)
								return true;

							tsplat = _mm_set_ps(-isect->t, -isect->t, 0.0f, 0.0f);
						}
#else
						if(hit && visibility == PATH_RAY_SHADOW_OPAQUE)
							return true;
#endif

						primAddr++;
					}
				}
#if FEATURE(BVH_INSTANCING)
				else {
					/* instance push */
					object = kernel_tex_fetch(__prim_object, -primAddr-1);

#if FEATURE(BVH_MOTION)
					bvh_instance_motion_push(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);
#else
					bvh_instance_push(kg, object, ray, &P, &idir, &isect->t, tmax);
#endif

#if defined(__KERNEL_SSE2__) && !FEATURE(BVH_HAIR_MINIMUM_WIDTH)
					Psplat[0] = _mm_set_ps1(P.x);
					Psplat[1] = _mm_set_ps1(P.y);
					Psplat[2] = _mm_set_ps1(P.z);

					idirsplat[0] = _mm_xor_ps(_mm_set_ps1(idir.x), pn);
					idirsplat[1] = _mm_xor_ps(_mm_set_ps1(idir.y), pn);
					idirsplat[2] = _mm_xor_ps(_mm_set_ps1(idir.z), pn);

					tsplat = _mm_set_ps(-isect->t, -isect->t, 0.0f, 0.0f);

					shufflex = (idir.x >= 0)? shuf_identity: shuf_swap;
					shuffley = (idir.y >= 0)? shuf_identity: shuf_swap;
					shufflez = (idir.z >= 0)? shuf_identity: shuf_swap;
#endif

					++stackPtr;
					traversalStack[stackPtr] = ENTRYPOINT_SENTINEL;

					nodeAddr = kernel_tex_fetch(__object_node, object);
				}
			}
#endif
		} while(nodeAddr != ENTRYPOINT_SENTINEL);

#if FEATURE(BVH_INSTANCING)
		if(stackPtr >= 0) {
			kernel_assert(object != ~0);

			/* instance pop */
#if FEATURE(BVH_MOTION)
			bvh_instance_motion_pop(kg, object, ray, &P, &idir, &isect->t, &ob_tfm, tmax);
#else
			bvh_instance_pop(kg, object, ray, &P, &idir, &isect->t, tmax);
#endif

#if defined(__KERNEL_SSE2__) && !FEATURE(BVH_HAIR_MINIMUM_WIDTH)
			Psplat[0] = _mm_set_ps1(P.x);
			Psplat[1] = _mm_set_ps1(P.y);
			Psplat[2] = _mm_set_ps1(P.z);

			idirsplat[0] = _mm_xor_ps(_mm_set_ps1(idir.x), pn);
			idirsplat[1] = _mm_xor_ps(_mm_set_ps1(idir.y), pn);
			idirsplat[2] = _mm_xor_ps(_mm_set_ps1(idir.z), pn);

			tsplat = _mm_set_ps(-isect->t, -isect->t, 0.0f, 0.0f);

			shufflex = (idir.x >= 0)? shuf_identity: shuf_swap;
			shuffley = (idir.y >= 0)? shuf_identity: shuf_swap;
			shufflez = (idir.z >= 0)? shuf_identity: shuf_swap;
#endif

			object = ~0;
			nodeAddr = traversalStack[stackPtr];
			--stackPtr;
		}
#endif
	} while(nodeAddr != ENTRYPOINT_SENTINEL);

	return (isect->prim != ~0);
}

#undef FEATURE
#undef BVH_FUNCTION_NAME
#undef BVH_FUNCTION_FEATURES
