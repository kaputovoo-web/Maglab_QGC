VARYING vec2 uv;
VARYING float terrainHeight;

void MAIN()
{
    uv = UV0;
    terrainHeight = VERTEX.z;
}
