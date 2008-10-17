#include "entities/TransportableItem.h"
#include "entities/Link.h"
#include "entities/MapEntities.h"
#include "movements/PathMovement.h"
#include "ResourceManager.h"
#include "Game.h"
#include "KeysEffect.h"
#include "Equipment.h"
#include "Map.h"
#include "Sound.h"
#include "ZSDX.h"
#include "Sprite.h"

/**
 * Properties of each type of transportable item.
 */
const TransportableItem::ItemProperties TransportableItem::properties[] = {
  {"entities/pot", "stone", 0},
  {"entities/skull", "stone", 0},
  {"entities/bush", "bush", 0},
  {"entities/stone_small_white", "stone", 1},
  {"entities/stone_small_white", "stone", 2},

  /* not implemented
  {"entities/stone_big_white", "stone", 1},
  {"entities/stone_big_white", "stone", 2},
  */
};

/**
 * Creates a new transportable item with the specified type.
 * @param map the map
 * @param layer layer of the transportable item to create on the map
 * @param x x coordinate of the transportable item to create
 * @param y y coordinate of the transportable item to create
 * @param type type of transportable item to create
 * @param pickable_item the type of pickable item that appears when the transportable
 * item is lifted
 * @param pickable_item_savegame_variable index of the savegame boolean variable
 * storing the possession state of the pickable item,
 * for certain kinds of pickable items only (a key, a piece of heart...)
 */
TransportableItem::TransportableItem(Map *map, Layer layer, int x, int y, TransportableItem::ItemType type,
				     PickableItem::ItemType pickable_item, int pickable_item_savegame_variable):
  Detector(COLLISION_FACING_POINT, "", layer, x, y, 16, 16),
  map(map), type(type), pickable_item(pickable_item),
  pickable_item_savegame_variable(pickable_item_savegame_variable), is_breaking(false) {

  set_origin(8, 13);
  create_sprite(get_animation_set_id());

  // additionnal behavior of a bush
  if (type == BUSH) {
    set_collision_mode(COLLISION_FACING_POINT | COLLISION_SPRITE);
  }
}

/**
 * Destructor.
 */
TransportableItem::~TransportableItem(void) {

}

/**
 * Returns the animation set of this transportable item.
 * @return the animations of the sprite
 */
string TransportableItem::get_animation_set_id(void) {
  return properties[type].animation_set_id;
}

/**
 * Returns the sound to play when this item is destroyed.
 * @return the sound to play when this item is destroyed
 */
Sound * TransportableItem::get_breaking_sound(void) {
  return ResourceManager::get_sound(properties[type].breaking_sound_id);
}

/**
 * This function is called by the engine when an entity overlaps the transportable item.
 * This is a redefinition of Detector::collision(MapEntity*).
 * If the entity is the hero, we allow him to lift the item.
 * @param entity_overlapping the entity overlapping the detector
 */
void TransportableItem::collision(MapEntity *entity_overlapping) {

  if (entity_overlapping->is_hero()) {

    Link *link = zsdx->game->get_link();
    KeysEffect *keys_effect = zsdx->game->get_keys_effect();
    Equipment *equipment = zsdx->game->get_equipment();

    int weight = properties[type].weight;

    if (keys_effect->get_action_key_effect() == KeysEffect::ACTION_KEY_NONE
	&& link->get_state() == Link::FREE
	&& equipment->can_lift(weight)
	&& !is_breaking) {

      keys_effect->set_action_key_effect(KeysEffect::ACTION_KEY_LIFT);
    }
  }
}

/**
 * This function is called by the engine when a sprite overlaps the transportable item.
 * This is a redefinition of Detector::collision(Sprite*).
 * If the sprite is the sword and this item is a bush, then the bush may be cut.
 * @param entity an entity
 * @param sprite_overlapping the sprite of this entity that is overlapping the detector
 */
void TransportableItem::collision(MapEntity *entity, Sprite *sprite_overlapping) {

  if (!is_breaking
      && entity->is_hero()
      && sprite_overlapping->get_animation_set_id().find("sword") != string::npos) {

    bool break_bush = false;
    Link *link = (Link*) entity;
    Link::State state = link->get_state();
    int animation_direction = link->get_animation_direction();
    int movement_direction = link->get_movement_direction();

    if (state == Link::SPIN_ATTACK) {
      break_bush = true;
    }
    else if (state == Link::SWORD_SWINGING
	     || movement_direction == animation_direction * 90) {

      SDL_Rect facing_point = link->get_facing_point();

      switch (animation_direction) {

      case 0:
	break_bush = facing_point.y >= position_in_map.y
	  && facing_point.y < position_in_map.y + position_in_map.h
	  && facing_point.x >= position_in_map.x - 14;
	break;

      case 1:
	break_bush = facing_point.x >= position_in_map.x
	  && facing_point.x < position_in_map.x + position_in_map.w
	  && facing_point.y < position_in_map.y + position_in_map.h + 14;
	break;

      case 2:
	break_bush = facing_point.y >= position_in_map.y
	  && facing_point.y < position_in_map.y + position_in_map.h
	  && facing_point.x < position_in_map.x + position_in_map.w + 14;
	break;

      case 3:
	break_bush = facing_point.x >= position_in_map.x
	  && facing_point.x < position_in_map.x + position_in_map.w
	  && facing_point.y >= position_in_map.y - 14 ;
	break;

      default:
	DIE("Invalid animation direction of Link: " << link->get_animation_direction());
	break;
      }
    }

    if (break_bush) {
      get_breaking_sound()->play();
      get_last_sprite()->set_current_animation("destroy");
      set_obstacle(OBSTACLE_NONE);
      is_breaking = true;

      if (pickable_item != PickableItem::NONE) {
	bool will_disappear = (pickable_item <= PickableItem::ARROW_10);
	map->get_entities()->add_pickable_item(get_layer(), get_x(), get_y(), pickable_item,
					       pickable_item_savegame_variable, MovementFalling::MEDIUM, will_disappear);
      }
    }
  }
}

/**
 * This function is called when the player presses the action key
 * when Link is facing this detector, and the action icon lets him do this.
 * Link lifts the item if possible.
 */
void TransportableItem::action_key_pressed(void) {

  KeysEffect *keys_effect = zsdx->game->get_keys_effect();
  Link *link = zsdx->game->get_link();

  if (keys_effect->get_action_key_effect() == KeysEffect::ACTION_KEY_LIFT && !is_breaking) {

    link->start_lifting(this);

    // play the sound
    ResourceManager::get_sound("lift")->play();

    // create the pickable item
    if (pickable_item != PickableItem::NONE) {
      bool will_disappear = (pickable_item <= PickableItem::ARROW_10);
      map->get_entities()->add_pickable_item(get_layer(), get_x(), get_y(), pickable_item,
					     pickable_item_savegame_variable, MovementFalling::MEDIUM, will_disappear);
    }

    // remove the item from the map
    map->get_entities()->remove_transportable_item(this);
  }
}

/**
 * Updates the item.
 */
void TransportableItem::update(void) {

  MapEntity::update();

  if (is_breaking && get_last_sprite()->is_over()) {

    // remove the item from the map
    map->get_entities()->remove_transportable_item(this);
  }
}
