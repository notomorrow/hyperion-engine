struct Material {
	float metallic;
	float roughness;
	float subsurface;
	float specular;
	float specularTint;
	float anisotropic;
	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;
};

uniform Material material;

vec4 packMaterial(Material mat)
{
	return vec4(0.0); // todo
}