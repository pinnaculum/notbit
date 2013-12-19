/*
 * Notbit - A Bitmessage client
 * Copyright (C) 2013  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ntb-keyring.h"
#include "ntb-util.h"
#include "ntb-main-context.h"
#include "ntb-log.h"
#include "ntb-crypto.h"
#include "ntb-key.h"
#include "ntb-list.h"
#include "ntb-store.h"
#include "ntb-pointer-list.h"

struct ntb_keyring {
        struct ntb_network *nw;
        struct ntb_crypto *crypto;
        struct ntb_list keys;
};

struct ntb_keyring_cookie {
        struct ntb_keyring *keyring;
        ntb_keyring_create_key_func func;
        void *user_data;
        struct ntb_crypto_cookie *crypto_cookie;
};

static void
save_keyring(struct ntb_keyring *keyring)
{
        struct ntb_key **keys;
        struct ntb_pointer_list *plist;
        int n_keys = 0, i = 0;

        ntb_list_for_each(plist, &keyring->keys, link)
                n_keys++;

        keys = alloca(sizeof (struct ntb_key *) * n_keys);

        ntb_list_for_each(plist, &keyring->keys, link)
                keys[i++] = plist->data;

        ntb_store_save_keys(NULL /* default store */, keys, n_keys);
}

static void
add_key(struct ntb_keyring *keyring,
        struct ntb_key *key)
{
        ntb_pointer_list_insert(&keyring->keys, ntb_key_ref(key));
}

static void
for_each_key_cb(struct ntb_key *key,
                void *user_data)
{
        struct ntb_keyring *keyring = user_data;

        add_key(keyring, key);
}

struct ntb_keyring *
ntb_keyring_new(struct ntb_network *nw)
{
        struct ntb_keyring *keyring;

        keyring = ntb_alloc(sizeof *keyring);

        keyring->nw = nw;
        keyring->crypto = ntb_crypto_new();
        ntb_list_init(&keyring->keys);

        ntb_store_for_each_key(NULL, /* default store */
                               for_each_key_cb,
                               keyring);

        return keyring;
}

static void
create_key_cb(struct ntb_key *key,
              void *user_data)
{
        struct ntb_keyring_cookie *cookie = user_data;
        struct ntb_keyring *keyring = cookie->keyring;

        add_key(keyring, key);
        save_keyring(keyring);

        if (cookie->func)
                cookie->func(key, cookie->user_data);

        ntb_free(cookie);
}

struct ntb_keyring_cookie *
ntb_keyring_create_key(struct ntb_keyring *keyring,
                       const char *label,
                       int leading_zeroes,
                       ntb_keyring_create_key_func func,
                       void *user_data)
{
        struct ntb_keyring_cookie *cookie;

        cookie = ntb_alloc(sizeof *cookie);
        cookie->keyring = keyring;
        cookie->func = func;
        cookie->user_data = user_data;

        cookie->crypto_cookie = ntb_crypto_create_key(keyring->crypto,
                                                      label,
                                                      leading_zeroes,
                                                      create_key_cb,
                                                      cookie);

        return cookie;
}

void
ntb_keyring_cancel_task(struct ntb_keyring_cookie *cookie)
{
        ntb_crypto_cancel_task(cookie->crypto_cookie);
        ntb_free(cookie);
}

void
ntb_keyring_free(struct ntb_keyring *keyring)
{
        struct ntb_pointer_list *plist, *tmp;

        ntb_list_for_each_safe(plist, tmp, &keyring->keys, link) {
                ntb_key_unref(plist->data);
                ntb_pointer_list_free(plist);
        }

        ntb_crypto_free(keyring->crypto);
        ntb_free(keyring);
}
