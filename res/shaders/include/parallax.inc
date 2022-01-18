uniform sampler2D ParallaxMap;
//uniform float u_parallaxHeight;
uniform int HasParallaxMap;

vec2 ParallaxMappingTexture(sampler2D parallaxTexture, vec2 texCoords, vec3 viewDir)
{
  float u_parallaxHeight = 0.025;
  // number of depth layers
  const float numLayers = 10;
  // calculate the size of each layer
  float layerDepth = 1.0 / numLayers;
  // depth of current layer
  float currentLayerDepth = 0.0;
  // the amount to shift the texture coordinates per layer (from vector P)
  vec2 P = viewDir.xy * u_parallaxHeight; 
  vec2 deltaTexCoords = P / numLayers;
  // get initial values
  vec2  currentTexCoords     = texCoords;
  float currentDepthMapValue = texture(parallaxTexture, currentTexCoords).r;
    
  while(currentLayerDepth < currentDepthMapValue)
  {
      // shift texture coordinates along direction of P
      currentTexCoords -= deltaTexCoords;
      // get depthmap value at current texture coordinates
      currentDepthMapValue = texture(ParallaxMap, currentTexCoords).r;  
      // get depth of next layer
      currentLayerDepth += layerDepth;  
  }
    // get texture coordinates before collision (reverse operations)
  vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

  // get depth after and before collision for linear interpolation
  float afterDepth  = currentDepthMapValue - currentLayerDepth;
  float beforeDepth = texture(parallaxTexture, prevTexCoords).r - currentLayerDepth + layerDepth;
   
  // interpolation of texture coordinates
  float weight = afterDepth / (afterDepth - beforeDepth);
  vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

  return finalTexCoords; 
}

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{
  return ParallaxMappingTexture(ParallaxMap, texCoords, viewDir);
}
