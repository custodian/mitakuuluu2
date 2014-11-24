/* Copyright 2013 Naikel Aparicio. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL EELI REILIN OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the author and should not be interpreted as representing
 * official policies, either expressed or implied, of the copyright holder.
 */

#ifndef GLOBALCONSTANTS_H
#define GLOBALCONSTANTS_H

#include <QStringList>

// WhatsApp Client Spoofing

#define USER_AGENT_VERSION_NOKIA  "2.12.15"
#define USER_AGENT_NOKIA    "WhatsApp/"USER_AGENT_VERSION_NOKIA" S40Version/gadCEX3.60 Device/Nokia302"
#define RESOURCE_NOKIA      "S40-"USER_AGENT_VERSION_NOKIA"-443"

#define USER_AGENT_VERSION  "2.11.596"
#define USER_AGENT          "WhatsApp/"USER_AGENT_VERSION" Android/4.2.1 Device/GalaxyS3"
#define RESOURCE            "Android-"USER_AGENT_VERSION"-443"

// WhatsApp Registration Key

#define BUILD_KEY   "PdA2DJyKoUrwLw1Bg6EIhzh502dF9noR9uFCllGk"
#define BUILD_HASH  "1391039105258"

#define ANDROID_S1  "y3QwH3tzYzCINwUANXrs2lu8FeKJPTXuuDYtRZjZ29xO79WJb9L98W9t4/fG8K0ytmgICFAfG+uuklMracYAEw=="
#define ANDROID_S2  "oR5adREZCVriXW9qXxCGsDHWf4jjV1+E0lxHL/KzsbYkhb/jBbiXmwUHiZ2smsdY3AJiYjp1cYHE+DlBA6xqeQ=="
#define ANDROID_S3  "MIIDMjCCAvCgAwIBAgIETCU2pDALBgcqhkjOOAQDBQAwfDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExFDASBgNVBAcTC1NhbnRhIENsYXJhMRYwFAYDVQQKEw1XaGF0c0FwcCBJbmMuMRQwEgYDVQQLEwtFbmdpbmVlcmluZzEUMBIGA1UEAxMLQnJpYW4gQWN0b24wHhcNMTAwNjI1MjMwNzE2WhcNNDQwMjE1MjMwNzE2WjB8MQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEUMBIGA1UEBxMLU2FudGEgQ2xhcmExFjAUBgNVBAoTDVdoYXRzQXBwIEluYy4xFDASBgNVBAsTC0VuZ2luZWVyaW5nMRQwEgYDVQQDEwtCcmlhbiBBY3RvbjCCAbgwggEsBgcqhkjOOAQBMIIBHwKBgQD9f1OBHXUSKVLfSpwu7OTn9hG3UjzvRADDHj+AtlEmaUVdQCJR+1k9jVj6v8X1ujD2y5tVbNeBO4AdNG/yZmC3a5lQpaSfn+gEexAiwk+7qdf+t8Yb+DtX58aophUPBPuD9tPFHsMCNVQTWhaRMvZ1864rYdcq7/IiAxmd0UgBxwIVAJdgUI8VIwvMspK5gqLrhAvwWBz1AoGBAPfhoIXWmz3ey7yrXDa4V7l5lK+7+jrqgvlXTAs9B4JnUVlXjrrUWU/mcQcQgYC0SRZxI+hMKBYTt88JMozIpuE8FnqLVHyNKOCjrh4rs6Z1kW6jfwv6ITVi8ftiegEkO8yk8b6oUZCJqIPf4VrlnwaSi2ZegHtVJWQBTDv+z0kqA4GFAAKBgQDRGYtLgWh7zyRtQainJfCpiaUbzjJuhMgo4fVWZIvXHaSHBU1t5w//S0lDK2hiqkj8KpMWGywVov9eZxZy37V26dEqr/c2m5qZ0E+ynSu7sqUD7kGx/zeIcGT0H+KAVgkGNQCo5Uc0koLRWYHNtYoIvt5R3X6YZylbPftF/8ayWTALBgcqhkjOOAQDBQADLwAwLAIUAKYCp0d6z4QQdyN74JDfQ2WCyi8CFDUM4CaNB+ceVXdKtOrNTQcc0e+t"
#define ANDROID_TT  "xOyKd7AoN0uoos7Fkeup5w=="

// WhatsApp URLs
#define URL_REGISTRATION_V2     "https://v.whatsapp.net/v2/"
#define URL_CONTACTS_AUTH       "https://sro.whatsapp.net/v2/sync/a"
#define URL_CONTACTS_SYNC       "https://sro.whatsapp.net/v2/sync/q"

// WhatsApp Servers
#define JID_DOMAIN              "s.whatsapp.net"
#define SERVER_DOMAIN           "c3.whatsapp.net"

// DBus Services

#define OBJECT_NAME "/"
#define SERVICE_NAME "harbour.mitakuuluu2.server"

// Settings

#define SETTINGS_ORGANIZATION               "coderus"
#define SETTINGS_COUNTERS                   "counters"

#define SETTINGS_SYNC                       "settings/sync"
#define SETTINGS_UNKNOWN                    "settings/acceptUnknown"
#define SETTINGS_AUTOMATIC_DOWNLOAD         "settings/automaticDownload"
#define SETTINGS_AUTOMATIC_DOWNLOAD_BYTES   "settings/automaticDownloadBytes"
#define SETTINGS_IMPORT_TO_GALLERY          "settings/importmediatogallery"
#define SETTINGS_LAST_SYNC                  "settings/lastsync"
#define SETTINGS_SYNC_FREQ                  "settings/syncfreq"

#define SETTINGS_WAVERSION                  "internal/waversion"

#define SETTINGS_SCRATCH1                   "internal/scratch1"
#define SETTINGS_SCRATCH2                   "internal/scratch2"
#define SETTINGS_SCRATCH3                   "internal/scratch3"
#define SETTINGS_SCRATCH4                   "internal/scratch4"

#define SETTINGS_SCRATCH5                   "internal/scratch5"
#define SETTINGS_SCRATCH6                   "internal/scratch6"

#define SETTINGS_EVIL                       "internal/evil"
#define SETTINGS_ANDROID                    "internal/android"

#define SETTINGS_NEXTCHALLENGE              "connection/nextchallenge"
#define SETTINGS_SERVER                     "connection/server"
#define SETTINGS_THREADING                  "connection/threading"

#define SETTINGS_PHONENUMBER                "account/phoneNumber"
#define SETTINGS_MYJID                      "account/myJid"
#define SETTINGS_PASSWORD                   "account/password"
#define SETTINGS_USERNAME                   "account/pushname"
#define SETTINGS_PRESENCE                   "account/presence"
#define SETTINGS_CC                         "account/cc"
#define SETTINGS_REGISTERED                 "account/registered"
#define SETTINGS_STATUS                     "account/status"
#define SETTINGS_CREATION                   "account/creation"
#define SETTINGS_EXPIRATION                 "account/expiration"
#define SETTINGS_ACCOUNTSTATUS              "account/accountstatus"
#define SETTINGS_KIND                       "account/kind"

// Default settings values

#define DEFAULT_AUTOMATIC_DOWNLOAD          524288
#define DEFAULT_IMPORT_TO_GALLERY           true
#define DEFAULT_SYNC_FREQ                   2

// Timers

#define MIN_INTERVAL                300000
#define RETRY_LOGIN_INTERVAL         10000
#define CHECK_QUEUE_INTERVAL          1000
#define CHECK_CONNECTION_INTERVAL   360000

// Directories

#define IMAGES_DIR      "/Pictures"
#define VIDEOS_DIR      "/Videos"
#define AUDIO_DIR       "/Music"
#define PHOTOS_DIR      "/Pictures"
#define DATA_DIR        "/Mitakuuluu"

// Extensions

#define EXTENSIONS_VIDEO   "Videos (*.avi *.mpg *.mpeg *.mpe *.3gp *.wmv *.mp4 *.mov *.qt *.AVI *.MPG *.MPEG *.MPE *.3GP *.WMV *.MP4 *.MOV *.QT)"
#define EXTENSIONS_IMAGE   "Images (*.png *.jpg *.jpeg *.gif *.PNG *.JPG *.JPEG *.GIF)"
#define EXTENSIONS_AUDIO   "Audio Files (*.aac *.mp3 *.m4a *.wma *.wav *.ogg *.AAC *.MP3 *.M4A *.WMA *.WAV *.OGG)"

#endif // GLOBALCONSTANTS_H
