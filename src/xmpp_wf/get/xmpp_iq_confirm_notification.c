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

#include <wb_tools.h>
#include <wb_xmpp.h>
#include <wb_session.h>
#include <wb_xmpp_wf.h>
#include <wb_cvar.h>
#include <wb_log.h>

static void confirm(const char *notif_id,
                    enum notif_type notif_type,
                    enum notif_result result)
{
    xmpp_send_iq_get(
        JID_MS(session.online.channel),
        NULL, NULL,
        "<query xmlns='urn:cryonline:k01'>"
        " <confirm_notification>"
        "  <notif id='%s' type='%d'>"
        "   <confirmation result='%d' status='%d'"
        "                 location=''/>"
        "  </notif>"
        " </confirm_notification>"
        "</query>",
        notif_id,
        notif_type,
        result,
        session.online.status);
}

void xmpp_iq_confirm_notification(const char *notif)
{
    char *notif_id = get_info(notif, "id='", "'", NULL);
    enum notif_type notif_type = get_info_int(notif, "type='", "'", NULL);

    switch (notif_type)
    {
        /* Confirm consecutive logins */
        case NOTIF_MESSAGE:
            xprintf("Message\n");
            confirm(notif_id, notif_type, NOTIF_ACCEPT);
            break;

        case NOTIF_NEW_RANK:
        {
            int rank = get_info_int(notif, "new_rank='", "'", NULL);

            xprintf("New rank reached: %d\n", rank);
            confirm(notif_id, notif_type, NOTIF_ACCEPT);
            break;
        }

        case NOTIF_UNLOCK_MISSION:
        {
            char *mission = get_info(notif, "unlocked_mission='", "'", NULL);

            xprintf("Unlocked mission: %s\n", mission);
            confirm(notif_id, notif_type, NOTIF_ACCEPT);

            free(mission);
            break;
        }

        case NOTIF_GIVE_MONEY:
        {
            /*
              <give_money currency='game_money' type='0' amount='100'>
                <consecutive_login_bonus previous_streak='0'
                                         previous_reward='0'
                                         current_streak='0'
                                         current_reward='0'/>
               </give_money>
            */
            char *currency = get_info(notif, "currency='", "'", NULL);
            unsigned int amount = get_info_int(notif, "amount='", "'", NULL);

            xprintf("Money given: %d %s\n", amount, currency);
            confirm(notif_id, notif_type, NOTIF_ACCEPT);

            if (0 == strcmp(currency, "game_money"))
                session.profile.money.game += amount;
            else if (0 == strcmp(currency, "crown_money"))
                session.profile.money.crown += amount;
            else if (0 == strcmp(currency, "cry_money"))
                session.profile.money.cry += amount;

            free(currency);
            break;
        }

        case NOTIF_GIVE_ITEM:
        {
            /*
              <give_item name='coin_01' offer_type='Consumable'
                         extended_time='72'
                         consumables_count='3'>
               <consecutive_login_bonus previous_streak='4'
                                        previous_reward='5'
                                        current_streak='5'
                                        current_reward='0'/>
              </give_item>
             */
            char *item = get_info(notif, "name='", "'", NULL);
            char *offer = get_info(notif, "offer_type='", "'", NULL);

            if (offer != NULL)
            {
                if (0 == strcmp(offer, "Consumable"))
                {
                    int consumables = get_info_int(notif, "consumables_count='", "'", NULL);

                    xprintf("Item given: %9d %s\n", consumables, item);
                }
                else if (0 == strcmp(offer, "Expiration"))
                {
                    int extended = get_info_int(notif, "extended_time='", "'", NULL);

                    xprintf("Item given: %8dh %s\n", extended, item);
                }
                else
                    xprintf("Item given: %s (%s)\n", item, offer);
            }
            else
            {
                xprintf("Item given: %s\n", item);
            }

            confirm(notif_id, notif_type, NOTIF_ACCEPT);

            free(item);
            free(offer);
            break;
        }

        case NOTIF_GIVE_RANDOM_BOX:
        {
            /*
              <give_random_box name='random_box_zsd_03'>
                <purchased_item>
                 <profile_item name='pt10_hlw01_shop'
                               profile_item_id='60094997'
                               offerId='0' added_expiration='1 day'
                               added_quantity='0' error_status='0'>
                  <item id='60094997' name='pt10_hlw01_shop'
                        attached_to='0'
                        config='dm=0;material=hlw;pocket_index=0'
                        slot='0' equipped='0' default='0' permanent='0'
                        expired_confirmed='0' buy_time_utc='1446492398'
                        expiration_time_utc='1468524707'
                        seconds_left='2422195'/>
                 </profile_item>
                 <crown_money name='crown_money_item_01' added='100' total='xxxx'/>
                </purchased_item>
              </give_random_box>
             */

            const char *m = strstr(notif, "<purchased_item");

            if (m != NULL)
            {
                unsigned total_xp = 0;
                unsigned total_crown = 0;

                m += sizeof ("<purchased_item");

                xprintf("Random box given:\n");

                do {

                    const char *exp_s = strstr(m, "<exp");
                    const char *crown_s = strstr(m, "<crown_money");
                    const char *profile_item_s = strstr(m, "<profile_item");

                    if (exp_s != NULL
                        && (profile_item_s == NULL || exp_s < profile_item_s)
                        && (crown_s == NULL || exp_s < crown_s))
                    {
                        m = exp_s + sizeof ("<exp");

                        total_xp += get_info_int(m, "added='", "'", NULL);
                    }
                    else if (crown_s != NULL
                        && (profile_item_s == NULL || crown_s < profile_item_s)
                        && (exp_s == NULL || crown_s < exp_s))
                    {
                        m = crown_s + sizeof ("<crown_money");

                        total_crown += get_info_int(m, "added='", "'", NULL);
                    }
                    else if (profile_item_s != NULL)
                    {
                        m = profile_item_s + sizeof ("<profile_item");

                        char *name = get_info(m, "name='", "'", NULL);
                        char *expir = get_info(m, "added_expiration='", "'", NULL);
                        char *quant = get_info(m, "added_quantity='", "'", NULL);

                        xprintf("RB Item: %9s %s\n",
                                expir && expir[0] != '0' ? expir
                                : quant && quant[0] != '0' ? quant
                                : "Permanent",
                                name);

                        free(quant);
                        free(expir);
                        free(name);
                    }
                    else
                    {
                        break;
                    }

                } while (1);

                if (total_xp != 0)
                        xprintf("RB Item: %9u XP\n",
                                total_xp);

                if (total_crown != 0)
                        xprintf("RB Item: %9u crown\n",
                                total_crown);

                session.profile.experience += total_xp;
                session.profile.money.crown += total_crown;
            }

            confirm(notif_id, notif_type, NOTIF_ACCEPT);
            break;
        }

        case NOTIF_CUSTOM_MESSAGE:
        {
            char *alert = get_info(notif, "data='", "'", NULL);

            xprintf("Notification: %s\n", alert);
            confirm(notif_id, notif_type, NOTIF_ACCEPT);

            free(alert);
            break;
        }

        /* Accept any friend requests */
        case NOTIF_FRIEND_REQUEST:
        {
            char *initiator =
                get_info(notif, "initiator='", "'", NULL);

            if (!cvar.wb_postpone_friend_requests)
            {
                confirm(notif_id, notif_type,
                        cvar.wb_accept_friend_requests
                        ? NOTIF_ACCEPT
                        : NOTIF_REFUSE);
            }

            xprintf("Friend request from %s (%s)\n",
                    initiator,
                    cvar.wb_postpone_friend_requests
                    ? "postponed"
                    : cvar.wb_accept_friend_requests
                    ? "accepted"
                    : "refused");

            free(initiator);
            break;
        }

        /* Accept any clan invites only if we don't already have one */
        case NOTIF_CLAN_INVITE:
        {
            if (session.profile.clan.id == 0)
            {
                char *initiator =
                    get_info(notif, "initiator='", "'", NULL);
                char *clan_name =
                    get_info(notif, "clan_name='", "'", NULL);

                if (!cvar.wb_postpone_clan_invites)
                {
                    confirm(notif_id, notif_type,
                            cvar.wb_accept_clan_invites
                            ? NOTIF_ACCEPT
                            : NOTIF_REFUSE);
                }

                xprintf("%s invites us to his clan '%s' (%s)\n",
                        initiator,
                        clan_name,
                        cvar.wb_postpone_clan_invites
                        ? "postponed"
                        : cvar.wb_accept_clan_invites
                        ? "accepted"
                        : "refused");

                free(initiator);
                free(clan_name);
            }
            break;
        }

        case NOTIF_CLAN_INVITE_RESULT:
        {
            char *nick = get_info(notif, "nickname='", "'", NULL);
            int result = get_info_int(notif, "result='", "'", NULL);

            switch (result)
            {
                case 0:
                    break;
                case 1:
                    xprintf("%s rejected the clan invitation\n", nick);
                    break;
                default:
                    xprintf("Failed to invite %s to clan (code: %d)\n",
                           nick, result);
                    break;
            }

            confirm(notif_id, notif_type, NOTIF_ACCEPT);

            free(nick);
            break;
        }

        case NOTIF_FRIEND_REQUEST_RESULT:
        {
            char *jid = get_info(notif, "jid='", "'", NULL);
            char *nick = get_info(notif, "nickname='", "'", NULL);
            char *pid = get_info(notif, "profile_id='", "'", NULL);
            int status = get_info_int(notif, "status='", "'", NULL);
            int exp = get_info_int(notif, "experience='", "'", NULL);
            int result = get_info_int(notif, "result='", "'", NULL);

            if (result == NOTIF_ACCEPT)
            {
                xprintf("%s accepted the friend request\n", nick);

                if (status <= STATUS_OFFLINE)
                    jid = NULL;

                struct friend * f = friend_list_add(jid,
                                                    nick,
                                                    pid,
                                                    status,
                                                    exp);
                xmpp_iq_peer_status_update(f);
            }
            else
            {
                xprintf("%s rejected the friend request\n", nick);
            }

            free(jid);
            free(nick);
            free(pid);

            break;
        }

        case NOTIF_ANNOUNCEMENT:
        case NOTIF_ACHIEVEMENT:
            confirm(notif_id, notif_type, NOTIF_ACCEPT);
            break;

        case NOTIF_DELETED_ITEM:
        {
            /* <item_deleted profile_item_id='50583363'/> */

            char *profile_item_id = get_info(notif, "profile_item_id='", "'", NULL);

            xprintf("Deleted item id %s\n", profile_item_id);

            free(profile_item_id);
        }

        default:
            break;
    }

    free(notif_id);
}
