# MapBuilder

A raylib heightmap creation and preview program, and other misc things for building a map for raylib games

Hey! Im Brandon. I just upgraded to a rpi5 (from the old rpi4). 

So far you will find the following programs after running sh build.sh
 - create is to create a map out of noise
 - play is to preview the map after its built
 - lod demonstrates the LOD system for map chunks, (it might be busted, its old at this point)
 - rock is a rock creator in progress, currently assets are not usable
 - model_test takes a model as the first argument and opens it (used for testing if a model will open, and what it will look like)
 - validate_tiles is used to make sure the system created valid batches of objects

[![Map_Preview_Example](z_week2.png)](z_week2.png)



The maps are fairly large and there are just too many triangles to render everything at full detail.

Instead I use a two different grid systems.
 - The first is the chunk grid, which manages the ground assets. (mountains, roads, ocean trenches, etc...).
    - 16x16 (256) chunks, roughly 1024 units of world space in each. 
    - uses an LOD system, sizes are 64(full), 32, 16, and 8.
        - each of these is produced from the same orignal hieght map data, but have smaller and smaller numbers of vertices
            - 8 is 8x8, 16 is 16x16, etc...
    - press L and you will see the 32 chunks colored blue, 16 colored purple, and 8 colored red.
    - "active" chunks are full 64 LOD, 3x3 grid centered at the players current chunk. Each level surrounds the next (most chunks are LOD 8)
    - [![Map_Chunk_LOD_Example](z_grid_lod.png)](z_grid_lod.png)
 - The second is a tile system for batching objects at a distance.
    - 8x8 grid on top of each chunk. I am still trying to figure out if 8x8 or 16x16 is better.
    - I think, the active tile grid around the player is 7x7, or 9x9, I cant remember which it ends up as (still have yet to write a good tool to see the tiles). 
        - Im happy with the size of the active grid
        - but Ive been noticing trees appear out of no where when you get close. It seems I have a tile bug to fix soon
        - there also might be a bug with the creation of these, they work pretty well, but the boxes around the tiles feel small...
    - LOD 64 chunks will render tiles
        - unless the tile is in the active tile grid, then it will render indivdual objects (right now just trees and rocks) at a higher level of detail
        - used in place of GPU instancing as I dont know hot to work with that for now
            - eventually I think I would like both techniques and we could really render some triangles to the screen
            - there is a trade off between tiles and GPU instancing
                - GPU instancing is a bit slower, but tiles/batches take a lot more memory (GPU and RAM).
    - [![Map_Tile_Boxes_Example](z_tile_boxes.png)](z_tile_boxes.png)









