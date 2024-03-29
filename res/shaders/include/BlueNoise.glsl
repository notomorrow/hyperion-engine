#ifndef BLUE_NOISE_GLSL
#define BLUE_NOISE_GLSL

// https://eheitzresearch.wordpress.com/762-2/
float SampleBlueNoise(int pixel_i, int pixel_j, int sample_index, int sample_dimension)
{
	// wrap arguments
	pixel_i = pixel_i & 127;
	pixel_j = pixel_j & 127;
	sample_index = sample_index & 255;
	sample_dimension = sample_dimension & 255;

	// xor index based on optimized ranking
	const uint ranking_tile_index = uint(sample_dimension + (pixel_i + pixel_j * 128)) * 8u;
	int ranked_sample_index = sample_index ^ ranking_tile[ranking_tile_index >> 2][ranking_tile_index & 3];

	const uint sobol_index = uint(sample_dimension + ranked_sample_index * 256);
	const uint scrambling_tile_index = uint((sample_dimension % 8) + (pixel_i + pixel_j * 128)) * 8u;

	// fetch value in sequence
	int value = sobol_256spp_256d[sobol_index >> 2][sobol_index & 3];

	// If the dimension is optimized, xor sequence value based on optimized scrambling
	value = value ^ scrambling_tile[scrambling_tile_index >> 2][scrambling_tile_index & 3];

	// convert to float and return
	float v = (0.5 + value) / 256.0;

	return v;
}


#endif
