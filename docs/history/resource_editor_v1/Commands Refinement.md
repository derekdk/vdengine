The load command should have a signature as follows:
load <canvasname> <imagename> <filepath>
if <canvasname> is not in the canvas list, create a new one with that name and the size of the loaded image
if <imagename> is not provided, use the filename (without extension) as the image name
resources in a canvas can be accessed outside of the canvas by referencing <canvasname>.<imagename>, so the image name just needs to be unique within the canvas, not globally unique.
<filepath> must be in quotes to identify it as a single argument, and can be an absolute path or relative to the current working directory of the application. The command should return an error if the file cannot be found or loaded as an image.
if a new canvas is created, the loaded image should be added as a resource to that canvas and displayed at 0,0 in layer 0. 
If the canvas already exists, the loaded image should be added as a new resource to that canvas not displayed.

The draw command will display an image in the canvas


draw <imagename> <canvasname>[layer] <position> <size>
eg: draw myimage mycanvas[0] 100,200 50,50
This will draw the resource "myimage" from the resource list into "mycanvas" on layer 0 at position (100,200) with size (50,50). The position is the top-left corner of the drawn image. The size is the width and height to draw the image (it can be different from the original image size to allow scaling). If the layer is not specified, it defaults to 0. if no canvas is specified, the seleced canvas/layer is used. The command should return an error if the specified image or canvas does not exist, or if the position or size arguments are not in the correct format.



create area <areaname> <canvasname> <position> <size>

images can be crystalized from an area in a canvas

create image <name> <canvasname>[layer1, layer2, ...] <areaname>
 Stores a new undisplayed image resource in the canvas. The image is created by compositing the specified layers in the canvas and cropping to the specified area. This allows you to create new images from parts of the canvas, which can then be drawn back into the canvas or other canvases. The command should return an error if the specified canvas, layers, or area do not exist, or if the position or size arguments are not in the correct format. Not specifying the layer will use all layers.

create layer <layername> <canvasname>[layer]
Creates a new layer in the canvas at the layer position specified, or if a layer exists there, inserts the new on at layer+1

