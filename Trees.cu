#define PRIM_TARGET_X 9

// this is the maximum number of calls that each tree can be from each other
#define TREE_CALL_RANGE 90

// I have data for 4 trees 
#define TREE_COUNT 4
//Based on tree count returns value for variable TARGET_TREE_FLAGS to validate possible //candidate seeds 
#define TARGET_TREE_FLAGS ((1 << TREE_COUNT) - 1)

// RNG stuff
#define MASK ((1LU << 48) - 1)
//Forward 1 call 
#define fwd_1(seed) (seed = (seed * 25214903917LU + 11LU) & MASK)
//Reverse 1 call
#define rev_1(seed) (seed = (seed * 246154705703781LU + 107048004364969LU) & MASK)
// Reverse 90  call 
#define rev_90(seed) (seed = (seed * 50455039039097LU + 259439823819518LU) & MASK)

// This is the initial filter stage that narrows down the entire set of seeds that are "potential" //candidates for Java Random’s original seed from 244 to however many seeds can generate this //tree that uses the tree with the most information.
kernel void filter_prim(global ulong *kernel_offset, global ulong *results_prim, volatile global uint *results_prim_count) {
 // the global ID of this kernel is the lower 44 bits of the seed that it's going to check. we just or //that with the target X value because we know the first seed for this tree should contain that //value
 ulong seed = (get_global_id(0) + *kernel_offset) | ((ulong) PRIM_TARGET_X << 44);

 // precalculated RNG steps for the primary tree; tree 0
// pos Z
 if ((((seed * 25214903917LU + 11LU) >> 44) & 15) != 13) return ; 
// base height - 4  
 if ((((seed * 55986898099985LU + 49720483695876LU) >> 47) & 1) != 1) return ; 
// radius > 1 
 if ((((seed * 120950523281469LU + 102626409374399LU) >> 46) & 3) < 1) return ; 
 if (((((seed * 205749139540585LU + 277363943098LU) & 281474976710655LU) >> 17) % 3) != 0) return ; // type
 if (((((seed * 233752471717045LU + 11718085204285LU) & 281474976710655LU) >> 17) % 5) != 2) return ; // height


 // if we make it past all those checks, save the seed and increment the counter for the seeds we //have found with the first filter
 results_prim[atomic_inc(results_prim_count)] = seed;
}


#define get_tree_flag(tree_flags, tree_id) ((tree_flags >> tree_id) & 1)
#define set_tree_flag(tree_flags, tree_id) (tree_flags |= 1 << tree_id)

#define check_tree(tree_id, target_x, target_z) {\
 if (get_tree_flag(tree_flags, tree_id) == 0\
 && tree_x == target_x\
 && tree_z == target_z\
 ) {\
 \
 tree_flags |= check_tree_aux_##tree_id(seed, iseed) << tree_id;\
 }\
}

uchar check_tree_1(ulong seed, ulong iseed) {
 // RNG steps for tree
 // tree 1 base 
 if ((((seed * 233752471717045LU + 11718085204285LU) >> 47) & 1) != 0) return 0; 
// type
 if (((((seed * 25214903917LU + 11LU) & 281474976710655LU) >> 17) % 3) == 0) return 0; 
 return 1;
}

uchar check_tree_2(ulong seed, ulong iseed) {
 // RNG steps for tree
 // tree 2 base 
 if ((((seed * 205749139540585LU + 277363943098LU) >> 46) & 3) != 2) return 0; 
 if ((((seed * 233752471717045LU + 11718085204285LU) >> 47) & 1) != 1) return 0; // base height
// initial radius
 if ((((seed * 120950523281469LU + 102626409374399LU) >> 47) & 1) != 0) return 0;  
 // type
if (((((seed * 25214903917LU + 11LU) & 281474976710655LU) >> 17) % 3) == 0) return 0;
 return 1;
}

uchar check_tree_3(ulong seed, ulong iseed) {
 //RNG steps for tree
 // tree 3
 if ((((seed * 233752471717045LU + 11718085204285LU) >> 47) & 1) != 1) return 0; // base height
 if (((((seed * 25214903917LU + 11LU) & 281474976710655LU) >> 17) % 3) != 0) return 0; // type
 if (((((seed * 205749139540585LU + 277363943098LU) & 281474976710655LU) >> 17) % 5) != 3) return 0; // height
 if (((((seed * 55986898099985LU + 49720483695876LU) & 281474976710655LU) >> 17) % 5) != 0) return 0; // radius == 1

 return 1;
}

uchar check_tree_4(ulong seed, ulong iseed) {
 // RNG steps for tree
 // tree 4
 if ((((seed * 233752471717045LU + 11718085204285LU) >> 47) & 1) != 0) return 0; // base height
 if (((((seed * 25214903917LU + 11LU) & 281474976710655LU) >> 17) % 3) == 0) return 0; // type

 return 1;
}
// this is the filter used to significantly narrow down the seeds we acquired with the first stage //using the rest of the trees in the population region we run this kernel once for every seed we //found in the first stage. 
kernel void filter(
 global ulong *results_prim,
 global uint *results_prim_count,
 global ulong *results_aux,
 volatile global uint *results_aux_count
) {
 ulong seed = results_prim[get_global_id(0)];
 ulong iseed = seed;

 // We check in a range of -220 to +220 around the primary tree seeds we found. We do this by //reversing that seed by 220 steps, then we just check all 440 seeds after that
 rev_90(seed);

 // Bit field for the trees that we find the reason we use a bit field instead of a counter is so we //can check if a tree has already been found so we don't check it twice and lose a couple cycles. 8 //bits should be plenty; but it is easy to change. 
 uchar tree_flags = 8;

 // we loop through all the possible places that the trees could be near the primary tree and check //them
 uchar tree_x, tree_z;
 for (int call_offset = 0; call_offset < TREE_CALL_RANGE * 2; call_offset++) {
 // nextInt(16) 
 tree_x = (fwd_1(seed) >> 44) & 15;
 // nextInt(16) 
 tree_z = (fwd_1(seed) >> 44) & 15;
//Leaves, explained in written Documentation 
check_tree_1(4, 2, 1);
check_tree_2(1, 3, 3);
check_tree_3(1, 3, 2);
check_tree_4(0, 2, 2);

 rev_1(seed);
 }

 // if all the flags are set, we found a very good candidate seed
 if (tree_flags == TARGET_TREE_FLAGS) {
 results_aux[atomic_inc(results_aux_count)] = iseed;
 }
}
