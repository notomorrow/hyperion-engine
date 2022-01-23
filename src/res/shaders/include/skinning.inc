in vec4 a_boneweights;
in vec4 a_boneindices;

uniform mat4 Bone[$NUM_BONES];

mat4 createSkinningMatrix()
{
	vec4 index = a_boneindices;
	vec4 weight = a_boneweights;

	mat4 skinning = mat4(0.0);

	int index0 = int(index.x);
	skinning += weight.x * Bone[index0];
	int index1 = int(index.y);
	skinning += weight.y * Bone[index1];
	int index2 = int(index.z);
	skinning += weight.z * Bone[index2];
	int index3 = int(index.w);
	skinning += weight.w * Bone[index3];

	return skinning;
}
