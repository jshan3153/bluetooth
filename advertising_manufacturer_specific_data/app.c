/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_assert.h"
#include "gatt_db.h"
#include "sl_simple_button_instances.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

#define NAME_MAX_LENGTH 20

typedef struct
{
  uint8_t len_flags;
  uint8_t type_flags;
  uint8_t val_flags;

  uint8_t len_manuf;
  uint8_t type_manuf;
  // First two bytes must contain the manufacturer ID (little-endian order)
  uint8_t company_LO;
  uint8_t company_HI;

  // The next bytes are freely configurable - using one byte for counter value and one byte for last button press
  uint8_t num_presses;
  uint8_t last_press;

  // length of the name AD element is variable, adding it last to keep things simple
  uint8_t len_name;
  uint8_t type_name;

  // NAME_MAX_LENGTH must be sized so that total length of data does not exceed 31 bytes
  char name[NAME_MAX_LENGTH];

  // These values are NOT included in the actual advertising payload, just for bookkeeping
  char dummy;        // Space for null terminator
  uint8_t data_size; // Actual length of advertising data
} CustomAdv_t;

CustomAdv_t advData;

uint8_t num_presses = 0;
uint8_t last_press = 0xFF;

void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID, uint8_t num_presses, uint8_t last_press, char *name)
{
  int n;

  pData->len_flags = 0x02;
  pData->type_flags = 0x01;
  pData->val_flags = flags;

  pData->len_manuf = 5;  // 1+2+2 bytes for type, company ID and the payload
  pData->type_manuf = 0xFF;
  pData->company_LO = companyID & 0xFF;
  pData->company_HI = (companyID >> 8) & 0xFF;

  pData->num_presses = num_presses;
  pData->last_press = last_press;

  // Name length, excluding null terminator
  n = strlen(name);
  if (n > NAME_MAX_LENGTH) {
    // Incomplete name
    pData->type_name = 0x08;
  } else {
    pData->type_name = 0x09;
  }

  strncpy(pData->name, name, NAME_MAX_LENGTH);

  if (n > NAME_MAX_LENGTH) {
    n = NAME_MAX_LENGTH;
  }

  pData->len_name = 1 + n; // length of name element is the name string length + 1 for the AD type

  // Calculate total length of advertising data
  pData->data_size = 3 + (1 + pData->len_manuf) + (1 + pData->len_name);
}

void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle, uint8_t num_presses, uint8_t last_press)
{
  sl_status_t sc;
  app_log("update_adv_data  ...\r\n");
  // Update the two variable fields in the custom advertising packet
  pData->num_presses = num_presses;
  pData->last_press = last_press;

  // Set custom advertising payload
  sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, 0, pData->data_size, (const uint8_t *)pData);
  app_log("sl_bt_legacy_advertiser_set_data  ...\r\n");
  app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set advertising data\n",
                  (int)sc);
}

static void handle_button_press(int button)
{
  sl_status_t sc;
  app_log("handle_button_press  ...\r\n");
  sl_bt_advertiser_stop(advertising_set_handle);
  app_log("sl_bt_advertiser_stop  ...\r\n");
  if (button > 0) {
    num_presses++;
  } else {
    if (num_presses > 0) num_presses--;
  }
  last_press = button;
  // Update the advertising data on-the-fly
  update_adv_data(&advData, advertising_set_handle, num_presses, last_press);

  // Restart advertising after client has disconnected.
  sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                     sl_bt_legacy_advertiser_connectable);
  app_log("sl_bt_legacy_advertiser_start  ...\r\n");
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to start advertising\n",
                (int)sc);

}
/******************************************
 * Callback when button state change
 ******************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if((handle == &sl_button_btn0) &&
      (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)) { //if button 0 is pressed
    sl_bt_external_signal(0x1);
  } else if ((handle == &sl_button_btn1) &&
      (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)) { //if button 1 is pressed
    sl_bt_external_signal(0x2);
    }
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t system_id[8];
  uint8_t address_type;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to get Bluetooth address\n",
                    (int)sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      app_log("system id : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ...\r\n", system_id[0],system_id[1],system_id[2],system_id[3],
              system_id[4],system_id[5],system_id[6],system_id[7]);
      //gattdb_system_id : gattdb.h gatt db handle
      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to write attribute\n",
                    (int)sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      //sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
      //app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising timing\n",
                    (int)sc);

      sl_bt_advertiser_set_channel_map(advertising_set_handle, 7);

      //void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID,
      //                     uint8_t num_presses, uint8_t last_press, char *name)
      fill_adv_packet(&advData, 0x06, 0x02FF, num_presses, last_press, "CustomAdvDemo");

      // Set custom advertising payload
      sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, 0, advData.data_size, (const uint8_t *)&advData);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising data\n",
                    (int)sc);

      // Start advertising using custom data
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to start advertising\n",
                      (int)sc);

      app_log("Start advertising ...\r\n");
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log("evt connection closed ...\r\n");
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to generate data\n",
                    (int)sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_system_external_signal_id:
      app_log("evt  ...\r\n");

      if (evt->data.evt_system_external_signal.extsignals & 0x1) {
          handle_button_press(0);
      }
      else if (evt->data.evt_system_external_signal.extsignals & 0x2) {
          handle_button_press(1);
      }
      break;
    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
