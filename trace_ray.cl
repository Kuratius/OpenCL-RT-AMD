#define uint32_t uint
struct vec{float x; float y; float z;};

struct radv_bvh_triangle_node {
   float coords[3][3];
   uint32_t reserved[3];
   uint32_t triangle_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
   uint32_t reserved2;
   uint32_t id;
};

struct radv_bvh_aabb_node {
   uint32_t primitive_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
   uint32_t reserved[14];
};

struct aabb {float minX; float minY; float minZ; float maxX; float maxY; float maxZ;};

struct radv_bvh_box32_node {
   uint32_t children[4];
   struct aabb coords[4];
   /* VK_BVH_BOX_FLAG_* indicating if all/no children are opaque */
   uint32_t flags;
   uint32_t reserved[3];
};


#if 0
//this code uses too many registers and triggers a compiler bug in instantiating the inline asm
//see https://github.com/llvm/llvm-project/issues/129071

static inline uint4 ray_intersect(uint32_t node_pointer,float ray_extent, float4 ray_origin,float4 ray_dir,float4 ray_inv_dir, uint4 texture_descriptor)
{
	    uint4 vout;	
	__asm__ volatile(
	    "image_bvh_intersect_ray %[vout], [%[node],%[vin1],%[vin2],%[vin3], %[vin4],%[vin5],%[vin6],%[vin7],%[vin8],%[vin9],%[vin10]],%[td]\n\t"
		:
        [vout]"=v"(vout) :
		[node]"v"(node_pointer),
		[vin1]"v"(ray_extent),
		[vin2]"v"(ray_origin[0]),
		[vin3]"v"(ray_origin[1]),
		[vin4]"v"(ray_origin[2]),
		[vin5]"v"(ray_dir[0]),
		[vin6]"v"(ray_dir[1]),
		[vin7]"v"(ray_dir[2]),
		[vin8]"v"(ray_inv_dir[0]),
		[vin9]"v"(ray_inv_dir[1]),
		[vin10]"v"(ray_inv_dir[2]),
        [td]"s"(texture_descriptor)
        :);
	return vout;
} 
#endif

__kernel void trace_ray(__global struct radv_bvh_box32_node* data, 
      __local float* local_result, __global float* group_result) {
   if (get_global_id(0)==0) {

        printf("Thread %lu \n", get_global_id(0));
		uint4 vout;
        printf("attempting rt\n");

        if (((ulong)data)  & 255 )
            printf("unaligned pointer!\n");
		uint32_t node_pointer=0x5 | (0<<3);
		float ray_extent=1/0.0;
		float4 ray_origin={0.5, 0,0,0};
		float4 ray_dir={1,0,0.0,0};
		float4 ray_inv_dir={1/ray_dir[0], 1/ray_dir[1],1/ray_dir[2], 0.0};
		uint4 texture_descriptor;
        texture_descriptor[0]=((ulong)data)>>8;
        texture_descriptor[1]= (1u<<31)| ((ulong)data)>>40;
        texture_descriptor[2]=~0;
        texture_descriptor[3]=0x8<<28 | ((~0u)>>22) ;
        vout=__builtin_amdgcn_image_bvh_intersect_ray_l(node_pointer, ray_extent,
           ray_origin, ray_dir, ray_inv_dir, texture_descriptor);

        printf("vout is %u %u %u %u\n", vout[0], vout[1], vout[2], vout[3]);
   }
}

