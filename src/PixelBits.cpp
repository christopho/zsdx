#include "PixelBits.h"

/**
 * Creates a pixel bits object.
 * @param surface the surface where the image is
 * @param image_position position of the image on this surface
 */
PixelBits::PixelBits(SDL_Surface *surface, SDL_Rect &image_position) {

  SDL_PixelFormat *format = surface->format;
  if (format->BitsPerPixel != 8) {
    DIE("This surface should have an 8-bit pixel format");
  }

  Uint8 colorkey = (Uint8) format->colorkey;

  width = image_position.w;
  height = image_position.h;

  nb_integers_per_row = width / 32;
  if (width % 32 != 0) {
    nb_integers_per_row++;
  }

  Uint8 *pixels = (Uint8*) surface->pixels;
  int pixel_index = image_position.y * surface->w + image_position.x;

  bits = new Uint32*[height];
  for (int i = 0; i < height; i++) {
    bits[i] = new Uint32[nb_integers_per_row];

    // fill the bits for this row
    int k = -1;
    Uint32 mask = 0x00000000;
    for (int j = 0; j < width; j++) {

      if (mask == 0x00000000) {
	k++;
	mask = 0x80000000;
	bits[i][k] = 0x00000000;
      }

      if (pixels[pixel_index] != colorkey) {
	bits[i][k] |= mask;
      }
      mask >>= 1;
      pixel_index++;
    }
    pixel_index += surface->w - width;
  }

  /* debug
  cout << "pixel bits initialized: frame size is " << width << " x " << height << endl;
  for (int i = 0; i < height; i++) {
    int k = -1;
    Uint32 mask = 0x00000000;
    for (int j = 0; j < width; j++) {

      if (mask == 0x00000000) {
	k++;
	mask = 0x80000000;
      }

      //      cout << "bit " << i << "," << j << " is " << (bits[i][k] & mask) << endl;
      
      if (bits[i][k] & mask) {
	cout << "X";
      }
      else {
	cout << ".";
      }

      mask >>= 1;      
    }
    cout << endl;
  }
  */
}

/**
 * Destructor.
 */
PixelBits::~PixelBits(void) {

  for (int i = 0; i < height; i++) {
    delete[] bits[i];
  }
  delete[] bits;
}

/**
 * Detects whether the image represented by these pixel bits is overlapping another image.
 * @param other the other image
 * @param location1 position of the top-left corner of this image on the map (only x and y must be specified)
 * @param location2 position of the top-left corner of the other image on the map (only x and y must be specified)
 * @return true if there is a collision
 */
bool PixelBits::check_collision(PixelBits *other, SDL_Rect &location1, SDL_Rect &location2) {

  // compute the two bounding boxes
  SDL_Rect bounding_box1 = location1;
  bounding_box1.w = width;
  bounding_box1.h = height;

  SDL_Rect bounding_box2 = location2;
  bounding_box2.w = other->width;
  bounding_box2.h = other->height;

  // check the collision between the two bounding boxes
  if (!check_rectangle_collision(bounding_box1, bounding_box2)) {
    return false;
  }

  // compute the intersection between the two rectangles
  SDL_Rect intersection;
  intersection.x = MAX(bounding_box1.x, bounding_box2.x);
  intersection.y = MAX(bounding_box1.y, bounding_box2.y);
  intersection.w = MIN(bounding_box1.x + bounding_box1.w, bounding_box2.x + bounding_box2.w) - intersection.x;
  intersection.h = MIN(bounding_box1.y + bounding_box1.h, bounding_box2.y + bounding_box2.h) - intersection.y;

  // compute the relative position of the intersection rectangle for each bounding_box
  int offset_x1 = intersection.x - bounding_box1.x;
  int offset_y1 = intersection.y - bounding_box1.y;

  int offset_x2 = intersection.x - bounding_box2.x;
  int offset_y2 = intersection.y - bounding_box2.y;

  /*
   * For each row of the intersection, we will call row 'a' the row coming from the right bounding box
   * and row 'b' the one coming from the left bounding box.
   */

  Uint32 **rows_a;           // for each row: array of masks of the right bounding box
  Uint32 **rows_b;           // for each row: array of masks of the left bounding box

  Uint32 *bits_a;            // array of masks of the right bounding box for the current row
  Uint32 *bits_b;            // array of masks of the left bounding box for the current row

  int nb_masks_per_row_a;    // number of masks on row a that are in the intersection (row b may have one more mask)
  bool has_row_b_additional_mask;
  int nb_unused_masks_row_b; // number of unused masks on row b (i.e. before the intersection)
  int nb_unused_bits_row_b;  // number of unused bits on the first used mask of row b
  int nb_used_bits_row_b;    // number of bits used on the first user mask of row b

  // compute the number of masks on row a
  nb_masks_per_row_a = intersection.w / 32;
  if (intersection.w % 32 != 0) {
    nb_masks_per_row_a++;
  }

  // make sure row a starts after row b
  if (bounding_box1.x > bounding_box2.x) {
    rows_a = &this->bits[offset_y1];
    rows_b = &other->bits[offset_y2];
    nb_unused_masks_row_b = offset_x2 / 32;
    nb_unused_bits_row_b = offset_x2 % 32;
    has_row_b_additional_mask = (nb_masks_per_row_a + nb_unused_masks_row_b + 1 < other->nb_integers_per_row);
  }
  else {
    rows_a = &other->bits[offset_y2];
    rows_b = &this->bits[offset_y1]; 
    nb_unused_masks_row_b = offset_x1 / 32;
    nb_unused_bits_row_b = offset_x1 % 32;
    has_row_b_additional_mask = (nb_masks_per_row_a + nb_unused_masks_row_b + 1 < this->nb_integers_per_row);
  }
  nb_used_bits_row_b = 32 - nb_unused_bits_row_b;

  // check the collisions each row of the intersection rectangle
  bool collision = false;
  for (int i = 0; i < intersection.h && !collision; i++) {

    // current row
    bits_a = rows_a[i];
    bits_b = rows_b[i];

    // check each mask
    for (int j = 0; j < nb_masks_per_row_a && !collision; j++) {

      /*
       * We compare a mask of row a with the right part of a mask of row b
       * and the left part of the next mask of row b
       */

      Uint32 mask_a = bits_a[j];
      Uint32 mask_b = bits_b[j + nb_unused_masks_row_b];
      Uint32 next_mask_b_left;

      if (has_row_b_additional_mask) {
	next_mask_b_left = bits_b[j + nb_unused_masks_row_b + 1] >> nb_used_bits_row_b;
      }
      else { // special case where the next mask b does not exist because a and b are aligned
	next_mask_b_left = 0x00000000;
      }

      Uint32 mask_a_left = mask_a >> nb_unused_bits_row_b;
      collision = ((mask_a_left & mask_b) | (mask_a & next_mask_b_left)) != 0x00000000;
    }
  }
 
  return collision;
}

/**
 * Checks whether two rectangles are overlapping.
 * @param rectangle1 first rectangle
 * @param rectangle2 second rectangle
 * @return true if there is a collision
 */
bool PixelBits::check_rectangle_collision(SDL_Rect &rectangle1, SDL_Rect &rectangle2) {

  int x1 = rectangle1.x;
  int x2 = x1 + rectangle1.w;
  int x3 = rectangle2.x;
  int x4 = x3 + rectangle2.w;

  bool overlap_x = (x3 < x2 && x1 < x4);

  int y1 = rectangle1.y;
  int y2 = y1 + rectangle1.h;
  int y3 = rectangle2.y;
  int y4 = y3 + rectangle2.h;

  bool overlap_y = (y3 < y2 && y1 < y4);

  return overlap_x && overlap_y;
}
