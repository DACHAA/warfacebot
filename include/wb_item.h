/**
 * WarfaceBot, a blind XMPP client for Warface (FPS)
 * Copyright (C) 2015, 2016 Levak Borok <levak92@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WB_ITEM_H
# define WB_ITEM_H

struct game_item
{
    unsigned int id;
    char *name;
    unsigned int locked;
    int equipped;
    int slot;

    char *config;
    char attached_to;
    char is_default;
    char permanent;
    char expired_confirmed;

    unsigned int buy_time_utc;
    unsigned int expiration_time_utc;
    unsigned int seconds_left;
    unsigned int quantity;
};

struct game_item *item_list_get(const char *resource);
struct game_item *item_list_get_by_type(const char *type);

typedef void (*f_pil_update_cb)(void *args);
struct list *item_list_new(void);
void item_list_update(f_pil_update_cb fun, void *args);
void item_list_init(struct list *items);
void item_list_free(void);

#endif /* !WB_ITEM_H */
