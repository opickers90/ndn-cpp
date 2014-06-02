/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2013-2014 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU General Public License is in the file COPYING.
 */

#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <stdexcept>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/sha256-with-rsa-signature.hpp>
#include <ndn-cpp/key-locator.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/encoding/binary-xml-wire-format.hpp>
#include <ndn-cpp/encoding/tlv-wire-format.hpp>
// Hack: Hook directly into non-API functions.
#include "../src/c/encoding/binary-xml-data.h"
#include "../src/c/encoding/tlv/tlv-data.h"
#include "../src/c/util/crypto.h"

using namespace std;
using namespace ndn;

static double
getNowSeconds()
{
  struct timeval t;
  gettimeofday(&t, 0);
  return t.tv_sec + t.tv_usec / 1000000.0;
}

static bool
verifyRsaSignature
  (const uint8_t* signedPortion, size_t signedPortionLength, const uint8_t* signatureBits, size_t signatureBitsLength, 
   const uint8_t* publicKeyDer, size_t publicKeyDerLength)
{
  // Set signedPortionDigest to the digest of the signed portion of the wire encoding.
  uint8_t signedPortionDigest[SHA256_DIGEST_LENGTH];
  ndn_digestSha256(signedPortion, signedPortionLength, signedPortionDigest);
  
  // Verify the signedPortionDigest.
  // Use a temporary pointer since d2i updates it.
  const uint8_t *derPointer = publicKeyDer;
  RSA *rsaPublicKey = d2i_RSA_PUBKEY(NULL, &derPointer, publicKeyDerLength);
  if (!rsaPublicKey) {
    // Don't expect this to happen.
    cout << "Error decoding public key in d2i_RSAPublicKey" << endl;
    return 0;
  }
  int success = RSA_verify
    (NID_sha256, signedPortionDigest, sizeof(signedPortionDigest), (uint8_t*)signatureBits, signatureBitsLength, rsaPublicKey);
  // Free the public key before checking for success.
  RSA_free(rsaPublicKey);
  
  // RSA_verify returns 1 for a valid signature.
  return (success == 1);
}

static uint8_t DEFAULT_PUBLIC_KEY_DER[] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
  0x00, 0xb8, 0x09, 0xa7, 0x59, 0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93,
  0x53, 0xbb, 0x7d, 0xd4, 0xac, 0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b,
  0x82, 0xca, 0xcd, 0x72, 0xce, 0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e,
  0xaf, 0xa7, 0xb3, 0x79, 0xbe, 0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe,
  0x3b, 0xce, 0x6e, 0xea, 0x07, 0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5,
  0xe1, 0xce, 0xe1, 0xd9, 0x8d, 0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c,
  0xd9, 0x7d, 0xbc, 0x96, 0xeb, 0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6,
  0x5d, 0xdb, 0xe1, 0xf6, 0xb1, 0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb,
  0xbe, 0xb3, 0x95, 0xca, 0xa5, 0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90,
  0xfd, 0x8a, 0x36, 0x35, 0x5e, 0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66,
  0xc1, 0x59, 0x3c, 0x41, 0x83, 0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61,
  0x74, 0xbe, 0x04, 0xf5, 0x7a, 0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d,
  0x85, 0x34, 0xfd, 0x02, 0x1a, 0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58,
  0xa7, 0x49, 0x34, 0x46, 0x61, 0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c,
  0x47, 0xcc, 0x5f, 0x99, 0x62, 0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04,
  0xfe, 0x15, 0x19, 0x1d, 0xdc, 0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92,
  0x41, 0x02, 0x03, 0x01, 0x00, 0x01
};

static uint8_t DEFAULT_PRIVATE_KEY_DER[] = {
  0x30, 0x82, 0x04, 0xa5, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00, 0xb8, 0x09, 0xa7, 0x59,
  0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93, 0x53, 0xbb, 0x7d, 0xd4, 0xac,
  0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b, 0x82, 0xca, 0xcd, 0x72, 0xce,
  0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e, 0xaf, 0xa7, 0xb3, 0x79, 0xbe,
  0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe, 0x3b, 0xce, 0x6e, 0xea, 0x07,
  0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5, 0xe1, 0xce, 0xe1, 0xd9, 0x8d,
  0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c, 0xd9, 0x7d, 0xbc, 0x96, 0xeb,
  0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6, 0x5d, 0xdb, 0xe1, 0xf6, 0xb1,
  0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb, 0xbe, 0xb3, 0x95, 0xca, 0xa5,
  0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90, 0xfd, 0x8a, 0x36, 0x35, 0x5e,
  0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66, 0xc1, 0x59, 0x3c, 0x41, 0x83,
  0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61, 0x74, 0xbe, 0x04, 0xf5, 0x7a,
  0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d, 0x85, 0x34, 0xfd, 0x02, 0x1a,
  0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58, 0xa7, 0x49, 0x34, 0x46, 0x61,
  0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c, 0x47, 0xcc, 0x5f, 0x99, 0x62,
  0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04, 0xfe, 0x15, 0x19, 0x1d, 0xdc,
  0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92, 0x41, 0x02, 0x03, 0x01, 0x00,
  0x01, 0x02, 0x82, 0x01, 0x01, 0x00, 0x8a, 0x05, 0xfb, 0x73, 0x7f, 0x16, 0xaf, 0x9f, 0xa9, 0x4c,
  0xe5, 0x3f, 0x26, 0xf8, 0x66, 0x4d, 0xd2, 0xfc, 0xd1, 0x06, 0xc0, 0x60, 0xf1, 0x9f, 0xe3, 0xa6,
  0xc6, 0x0a, 0x48, 0xb3, 0x9a, 0xca, 0x21, 0xcd, 0x29, 0x80, 0x88, 0x3d, 0xa4, 0x85, 0xa5, 0x7b,
  0x82, 0x21, 0x81, 0x28, 0xeb, 0xf2, 0x43, 0x24, 0xb0, 0x76, 0xc5, 0x52, 0xef, 0xc2, 0xea, 0x4b,
  0x82, 0x41, 0x92, 0xc2, 0x6d, 0xa6, 0xae, 0xf0, 0xb2, 0x26, 0x48, 0xa1, 0x23, 0x7f, 0x02, 0xcf,
  0xa8, 0x90, 0x17, 0xa2, 0x3e, 0x8a, 0x26, 0xbd, 0x6d, 0x8a, 0xee, 0xa6, 0x0c, 0x31, 0xce, 0xc2,
  0xbb, 0x92, 0x59, 0xb5, 0x73, 0xe2, 0x7d, 0x91, 0x75, 0xe2, 0xbd, 0x8c, 0x63, 0xe2, 0x1c, 0x8b,
  0xc2, 0x6a, 0x1c, 0xfe, 0x69, 0xc0, 0x44, 0xcb, 0x58, 0x57, 0xb7, 0x13, 0x42, 0xf0, 0xdb, 0x50,
  0x4c, 0xe0, 0x45, 0x09, 0x8f, 0xca, 0x45, 0x8a, 0x06, 0xfe, 0x98, 0xd1, 0x22, 0xf5, 0x5a, 0x9a,
  0xdf, 0x89, 0x17, 0xca, 0x20, 0xcc, 0x12, 0xa9, 0x09, 0x3d, 0xd5, 0xf7, 0xe3, 0xeb, 0x08, 0x4a,
  0xc4, 0x12, 0xc0, 0xb9, 0x47, 0x6c, 0x79, 0x50, 0x66, 0xa3, 0xf8, 0xaf, 0x2c, 0xfa, 0xb4, 0x6b,
  0xec, 0x03, 0xad, 0xcb, 0xda, 0x24, 0x0c, 0x52, 0x07, 0x87, 0x88, 0xc0, 0x21, 0xf3, 0x02, 0xe8,
  0x24, 0x44, 0x0f, 0xcd, 0xa0, 0xad, 0x2f, 0x1b, 0x79, 0xab, 0x6b, 0x49, 0x4a, 0xe6, 0x3b, 0xd0,
  0xad, 0xc3, 0x48, 0xb9, 0xf7, 0xf1, 0x34, 0x09, 0xeb, 0x7a, 0xc0, 0xd5, 0x0d, 0x39, 0xd8, 0x45,
  0xce, 0x36, 0x7a, 0xd8, 0xde, 0x3c, 0xb0, 0x21, 0x96, 0x97, 0x8a, 0xff, 0x8b, 0x23, 0x60, 0x4f,
  0xf0, 0x3d, 0xd7, 0x8f, 0xf3, 0x2c, 0xcb, 0x1d, 0x48, 0x3f, 0x86, 0xc4, 0xa9, 0x00, 0xf2, 0x23,
  0x2d, 0x72, 0x4d, 0x66, 0xa5, 0x01, 0x02, 0x81, 0x81, 0x00, 0xdc, 0x4f, 0x99, 0x44, 0x0d, 0x7f,
  0x59, 0x46, 0x1e, 0x8f, 0xe7, 0x2d, 0x8d, 0xdd, 0x54, 0xc0, 0xf7, 0xfa, 0x46, 0x0d, 0x9d, 0x35,
  0x03, 0xf1, 0x7c, 0x12, 0xf3, 0x5a, 0x9d, 0x83, 0xcf, 0xdd, 0x37, 0x21, 0x7c, 0xb7, 0xee, 0xc3,
  0x39, 0xd2, 0x75, 0x8f, 0xb2, 0x2d, 0x6f, 0xec, 0xc6, 0x03, 0x55, 0xd7, 0x00, 0x67, 0xd3, 0x9b,
  0xa2, 0x68, 0x50, 0x6f, 0x9e, 0x28, 0xa4, 0x76, 0x39, 0x2b, 0xb2, 0x65, 0xcc, 0x72, 0x82, 0x93,
  0xa0, 0xcf, 0x10, 0x05, 0x6a, 0x75, 0xca, 0x85, 0x35, 0x99, 0xb0, 0xa6, 0xc6, 0xef, 0x4c, 0x4d,
  0x99, 0x7d, 0x2c, 0x38, 0x01, 0x21, 0xb5, 0x31, 0xac, 0x80, 0x54, 0xc4, 0x18, 0x4b, 0xfd, 0xef,
  0xb3, 0x30, 0x22, 0x51, 0x5a, 0xea, 0x7d, 0x9b, 0xb2, 0x9d, 0xcb, 0xba, 0x3f, 0xc0, 0x1a, 0x6b,
  0xcd, 0xb0, 0xe6, 0x2f, 0x04, 0x33, 0xd7, 0x3a, 0x49, 0x71, 0x02, 0x81, 0x81, 0x00, 0xd5, 0xd9,
  0xc9, 0x70, 0x1a, 0x13, 0xb3, 0x39, 0x24, 0x02, 0xee, 0xb0, 0xbb, 0x84, 0x17, 0x12, 0xc6, 0xbd,
  0x65, 0x73, 0xe9, 0x34, 0x5d, 0x43, 0xff, 0xdc, 0xf8, 0x55, 0xaf, 0x2a, 0xb9, 0xe1, 0xfa, 0x71,
  0x65, 0x4e, 0x50, 0x0f, 0xa4, 0x3b, 0xe5, 0x68, 0xf2, 0x49, 0x71, 0xaf, 0x15, 0x88, 0xd7, 0xaf,
  0xc4, 0x9d, 0x94, 0x84, 0x6b, 0x5b, 0x10, 0xd5, 0xc0, 0xaa, 0x0c, 0x13, 0x62, 0x99, 0xc0, 0x8b,
  0xfc, 0x90, 0x0f, 0x87, 0x40, 0x4d, 0x58, 0x88, 0xbd, 0xe2, 0xba, 0x3e, 0x7e, 0x2d, 0xd7, 0x69,
  0xa9, 0x3c, 0x09, 0x64, 0x31, 0xb6, 0xcc, 0x4d, 0x1f, 0x23, 0xb6, 0x9e, 0x65, 0xd6, 0x81, 0xdc,
  0x85, 0xcc, 0x1e, 0xf1, 0x0b, 0x84, 0x38, 0xab, 0x93, 0x5f, 0x9f, 0x92, 0x4e, 0x93, 0x46, 0x95,
  0x6b, 0x3e, 0xb6, 0xc3, 0x1b, 0xd7, 0x69, 0xa1, 0x0a, 0x97, 0x37, 0x78, 0xed, 0xd1, 0x02, 0x81,
  0x80, 0x33, 0x18, 0xc3, 0x13, 0x65, 0x8e, 0x03, 0xc6, 0x9f, 0x90, 0x00, 0xae, 0x30, 0x19, 0x05,
  0x6f, 0x3c, 0x14, 0x6f, 0xea, 0xf8, 0x6b, 0x33, 0x5e, 0xee, 0xc7, 0xf6, 0x69, 0x2d, 0xdf, 0x44,
  0x76, 0xaa, 0x32, 0xba, 0x1a, 0x6e, 0xe6, 0x18, 0xa3, 0x17, 0x61, 0x1c, 0x92, 0x2d, 0x43, 0x5d,
  0x29, 0xa8, 0xdf, 0x14, 0xd8, 0xff, 0xdb, 0x38, 0xef, 0xb8, 0xb8, 0x2a, 0x96, 0x82, 0x8e, 0x68,
  0xf4, 0x19, 0x8c, 0x42, 0xbe, 0xcc, 0x4a, 0x31, 0x21, 0xd5, 0x35, 0x6c, 0x5b, 0xa5, 0x7c, 0xff,
  0xd1, 0x85, 0x87, 0x28, 0xdc, 0x97, 0x75, 0xe8, 0x03, 0x80, 0x1d, 0xfd, 0x25, 0x34, 0x41, 0x31,
  0x21, 0x12, 0x87, 0xe8, 0x9a, 0xb7, 0x6a, 0xc0, 0xc4, 0x89, 0x31, 0x15, 0x45, 0x0d, 0x9c, 0xee,
  0xf0, 0x6a, 0x2f, 0xe8, 0x59, 0x45, 0xc7, 0x7b, 0x0d, 0x6c, 0x55, 0xbb, 0x43, 0xca, 0xc7, 0x5a,
  0x01, 0x02, 0x81, 0x81, 0x00, 0xab, 0xf4, 0xd5, 0xcf, 0x78, 0x88, 0x82, 0xc2, 0xdd, 0xbc, 0x25,
  0xe6, 0xa2, 0xc1, 0xd2, 0x33, 0xdc, 0xef, 0x0a, 0x97, 0x2b, 0xdc, 0x59, 0x6a, 0x86, 0x61, 0x4e,
  0xa6, 0xc7, 0x95, 0x99, 0xa6, 0xa6, 0x55, 0x6c, 0x5a, 0x8e, 0x72, 0x25, 0x63, 0xac, 0x52, 0xb9,
  0x10, 0x69, 0x83, 0x99, 0xd3, 0x51, 0x6c, 0x1a, 0xb3, 0x83, 0x6a, 0xff, 0x50, 0x58, 0xb7, 0x28,
  0x97, 0x13, 0xe2, 0xba, 0x94, 0x5b, 0x89, 0xb4, 0xea, 0xba, 0x31, 0xcd, 0x78, 0xe4, 0x4a, 0x00,
  0x36, 0x42, 0x00, 0x62, 0x41, 0xc6, 0x47, 0x46, 0x37, 0xea, 0x6d, 0x50, 0xb4, 0x66, 0x8f, 0x55,
  0x0c, 0xc8, 0x99, 0x91, 0xd5, 0xec, 0xd2, 0x40, 0x1c, 0x24, 0x7d, 0x3a, 0xff, 0x74, 0xfa, 0x32,
  0x24, 0xe0, 0x11, 0x2b, 0x71, 0xad, 0x7e, 0x14, 0xa0, 0x77, 0x21, 0x68, 0x4f, 0xcc, 0xb6, 0x1b,
  0xe8, 0x00, 0x49, 0x13, 0x21, 0x02, 0x81, 0x81, 0x00, 0xb6, 0x18, 0x73, 0x59, 0x2c, 0x4f, 0x92,
  0xac, 0xa2, 0x2e, 0x5f, 0xb6, 0xbe, 0x78, 0x5d, 0x47, 0x71, 0x04, 0x92, 0xf0, 0xd7, 0xe8, 0xc5,
  0x7a, 0x84, 0x6b, 0xb8, 0xb4, 0x30, 0x1f, 0xd8, 0x0d, 0x58, 0xd0, 0x64, 0x80, 0xa7, 0x21, 0x1a,
  0x48, 0x00, 0x37, 0xd6, 0x19, 0x71, 0xbb, 0x91, 0x20, 0x9d, 0xe2, 0xc3, 0xec, 0xdb, 0x36, 0x1c,
  0xca, 0x48, 0x7d, 0x03, 0x32, 0x74, 0x1e, 0x65, 0x73, 0x02, 0x90, 0x73, 0xd8, 0x3f, 0xb5, 0x52,
  0x35, 0x79, 0x1c, 0xee, 0x93, 0xa3, 0x32, 0x8b, 0xed, 0x89, 0x98, 0xf1, 0x0c, 0xd8, 0x12, 0xf2,
  0x89, 0x7f, 0x32, 0x23, 0xec, 0x67, 0x66, 0x52, 0x83, 0x89, 0x99, 0x5e, 0x42, 0x2b, 0x42, 0x4b,
  0x84, 0x50, 0x1b, 0x3e, 0x47, 0x6d, 0x74, 0xfb, 0xd1, 0xa6, 0x10, 0x20, 0x6c, 0x6e, 0xbe, 0x44,
  0x3f, 0xb9, 0xfe, 0xbc, 0x8d, 0xda, 0xcb, 0xea, 0x8f
};

/**
 * Loop to encode a data packet nIterations times using C++.
 * @param nIterations The number of iterations.
 * @param useComplex If true, use a large name, large content and all fields.  If false, use a small name, small content
 * and only required fields.
 * @param useCrypto If true, sign the data packet.  If false, use a blank signature.
 * @param encoding Set this to the wire encoding.
 * @return The number of seconds for all iterations.
 */
static double
benchmarkEncodeDataSecondsCpp(int nIterations, bool useComplex, bool useCrypto, Blob& encoding)
{
  Name name;
  Blob content;
  if (useComplex) {
    // Use a large name and content.
    name = Name("/ndn/ucla.edu/apps/lwndn-test/numbers.txt/%FD%05%05%E8%0C%CE%1D/%00"); 
    
    ostringstream contentStream;
    int count = 1;
    contentStream << (count++);
    while (contentStream.str().length() < 1115)
      contentStream << " " << (count++);
    content = Blob((uint8_t*)contentStream.str().c_str(), contentStream.str().length());
  }
  else {
    // Use a small name and content.
    name = Name("/test");
    content = Blob((uint8_t*)"abc", 3);
  }
  Blob finalBlockId((uint8_t*)"\x00", 1);
  
  // Initialize the KeyChain storage in case useCrypto is true.
  ptr_lib::shared_ptr<MemoryIdentityStorage> identityStorage(new MemoryIdentityStorage());
  ptr_lib::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(new MemoryPrivateKeyStorage());
  KeyChain keyChain
    (ptr_lib::make_shared<IdentityManager>(identityStorage, privateKeyStorage), 
     ptr_lib::make_shared<SelfVerifyPolicyManager>(identityStorage.get()));
  Name keyName("/testname/DSK-123");
  Name certificateName = keyName.getSubName(0, keyName.size() - 1).append("KEY").append
    (keyName.get(keyName.size() - 1)).append("ID-CERT").append("0");
  privateKeyStorage->setKeyPairForKeyName
    (keyName, DEFAULT_PUBLIC_KEY_DER, sizeof(DEFAULT_PUBLIC_KEY_DER), DEFAULT_PRIVATE_KEY_DER, sizeof(DEFAULT_PRIVATE_KEY_DER));
  
  // Set up publisherPublicKeyDigest and signatureBits in case useCrypto is false.
  uint8_t publisherPublicKeyDigestArray[32];
  memset(publisherPublicKeyDigestArray, 0, sizeof(publisherPublicKeyDigestArray));
  Blob publisherPublicKeyDigest(publisherPublicKeyDigestArray, sizeof(publisherPublicKeyDigestArray));
  uint8_t signatureBitsArray[256];
  memset(signatureBitsArray, 0, sizeof(signatureBitsArray));
  Blob signatureBits(signatureBitsArray, sizeof(signatureBitsArray));

  double start = getNowSeconds();
  for (int i = 0; i < nIterations; ++i) {
    Data data(name);
    data.setContent(content);
    if (useComplex) {
      data.getMetaInfo().setFreshnessPeriod(1000);
      data.getMetaInfo().setFinalBlockID(finalBlockId);
    }

    if (useCrypto)
      // This sets the signature fields.
      keyChain.sign(data, certificateName);
    else {
      // Imitate IdentityManager::signByCertificate to set up the signature fields, but don't sign.
      KeyLocator keyLocator;    
      keyLocator.setType(ndn_KeyLocatorType_KEYNAME);
      keyLocator.setKeyName(certificateName);
      Sha256WithRsaSignature* sha256Signature = (Sha256WithRsaSignature*)data.getSignature();
      sha256Signature->setKeyLocator(keyLocator);
      sha256Signature->getPublisherPublicKeyDigest().setPublisherPublicKeyDigest(publisherPublicKeyDigest);
      sha256Signature->setSignature(signatureBits);
    }

    encoding = data.wireEncode();
  }
  double finish = getNowSeconds();
    
  return finish - start;
}

static void 
onVerified(const ptr_lib::shared_ptr<Data>& data)
{
  // Do nothing since we expect it to verify.
}

static void 
onVerifyFailed(const ptr_lib::shared_ptr<Data>& data)
{
  cout << "Signature verification: FAILED" << endl;
}

/**
 * Loop to decode a data packet nIterations times using C++.
 * @param nIterations The number of iterations.
 * @param useCrypto If true, verify the signature.  If false, don't verify.
 * @param encoding The wire encoding to decode.
 * @return The number of seconds for all iterations.
 */
static double 
benchmarkDecodeDataSecondsCpp(int nIterations, bool useCrypto, const Blob& encoding)
{
  // Initialize the KeyChain storage in case useCrypto is true.
  ptr_lib::shared_ptr<MemoryIdentityStorage> identityStorage(new MemoryIdentityStorage());
  ptr_lib::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(new MemoryPrivateKeyStorage());
  KeyChain keyChain
    (ptr_lib::make_shared<IdentityManager>(identityStorage, privateKeyStorage), 
     ptr_lib::make_shared<SelfVerifyPolicyManager>(identityStorage.get()));
  Name keyName("/testname/DSK-123");
  identityStorage->addKey(keyName, KEY_TYPE_RSA, Blob(DEFAULT_PUBLIC_KEY_DER, sizeof(DEFAULT_PUBLIC_KEY_DER)));

  double start = getNowSeconds();
  for (int i = 0; i < nIterations; ++i) {
    ptr_lib::shared_ptr<Data> data(new Data());
    data->wireDecode(encoding);
    
    if (useCrypto)
      keyChain.verifyData(data, onVerified, onVerifyFailed);
  }
  double finish = getNowSeconds();
 
  return finish - start;
}

/**
 * Loop to encode a data packet nIterations times using C.
 * @param nIterations The number of iterations.
 * @param useComplex If true, use a large name, large content and all fields.  If false, use a small name, small content
 * and only required fields.
 * @param useCrypto If true, sign the data packet.  If false, use a blank signature.
 * @param encoding Output buffer for the wire encoding.
 * @param maxEncodingLength The size of the encoding buffer.
 * @param encodingLength Return the number of output bytes in encoding.
 * @return The number of seconds for all iterations.
 */
static double 
benchmarkEncodeDataSecondsC
  (int nIterations, bool useComplex, bool useCrypto, uint8_t* encoding, size_t maxEncodingLength, size_t *encodingLength)
{
  struct ndn_Blob finalBlockId;
  ndn_Blob_initialize(&finalBlockId, (uint8_t*)"\x00", 1);

  struct ndn_NameComponent nameComponents[20];
  struct ndn_Name name;
  ndn_Name_initialize(&name, nameComponents, sizeof(nameComponents) / sizeof(nameComponents[0]));
  Blob contentBlob;
  struct ndn_Blob content;
  if (useComplex) {
    // Use a large name and content.
    ndn_Name_appendString(&name, (char*)"ndn");
    ndn_Name_appendString(&name, (char*)"ucla.edu");
    ndn_Name_appendString(&name, (char*)"apps");
    ndn_Name_appendString(&name, (char*)"lwndn-test");
    ndn_Name_appendString(&name, (char*)"numbers.txt");
    ndn_Name_appendString(&name, (char*)"\xFD\x05\x05\xE8\x0C\xCE\x1D");
    ndn_Name_appendBlob(&name, &finalBlockId);
    
    ostringstream contentStream;
    int count = 1;
    contentStream << (count++);
    while (contentStream.str().length() < 1115)
      contentStream << " " << (count++);
    contentBlob = Blob((uint8_t*)contentStream.str().c_str(), contentStream.str().length());
  }
  else {
    // Use a small name and content.
    ndn_Name_appendString(&name, (char*)"test");
    contentBlob = Blob((uint8_t*)"abc", 3);
  }
  ndn_Blob_initialize(&content, (uint8_t*)contentBlob.buf(), contentBlob.size());
  
  struct ndn_NameComponent certificateNameComponents[20];
  struct ndn_Name certificateName;
  ndn_Name_initialize(&certificateName, certificateNameComponents, sizeof(certificateNameComponents) / sizeof(certificateNameComponents[0]));
  ndn_Name_appendString(&certificateName, (char*)"testname");
  ndn_Name_appendString(&certificateName, (char*)"KEY");
  ndn_Name_appendString(&certificateName, (char*)"DSK-123");
  ndn_Name_appendString(&certificateName, (char*)"ID-CERT");
  ndn_Name_appendString(&certificateName, (char*)"0");
  
  // Set up publisherPublicKeyDigest and signatureBits in case useCrypto is false.
  uint8_t* publicKeyDer = DEFAULT_PUBLIC_KEY_DER;
  size_t publicKeyDerLength = sizeof(DEFAULT_PUBLIC_KEY_DER);
  uint8_t publisherPublicKeyDigestArray[SHA256_DIGEST_LENGTH];
  ndn_digestSha256(publicKeyDer, publicKeyDerLength, publisherPublicKeyDigestArray);
  struct ndn_Blob publisherPublicKeyDigest;
  ndn_Blob_initialize(&publisherPublicKeyDigest, publisherPublicKeyDigestArray, sizeof(publisherPublicKeyDigestArray));
  uint8_t signatureBitsArray[256];
  memset(signatureBitsArray, 0, sizeof(signatureBitsArray));
  
  // Set up the private key now in case useCrypto is true.
  // Use a temporary pointer since d2i updates it.
  const uint8_t *privateKeyDerPointer = DEFAULT_PRIVATE_KEY_DER;
  RSA *privateKey = d2i_RSAPrivateKey(NULL, &privateKeyDerPointer, sizeof(DEFAULT_PRIVATE_KEY_DER));
  if (!privateKey) {
    // Don't expect this to happen.
    cout << "Error decoding private key DER" << endl;
    return 0;
  }
  
  double start = getNowSeconds();
  for (int i = 0; i < nIterations; ++i) {
    struct ndn_Data data;
    ndn_Data_initialize(&data, name.components, name.maxComponents, certificateName.components, certificateName.maxComponents);
    
    data.name = name;
    data.content = content;
    if (useComplex) {
      data.metaInfo.timestampMilliseconds = 1.3e+12;
      data.metaInfo.freshnessPeriod = 1000;
      ndn_NameComponent_initialize(&data.metaInfo.finalBlockID, finalBlockId.value, finalBlockId.length);
    }

    struct ndn_DynamicUInt8Array output;
    struct ndn_BinaryXmlEncoder binaryXmlEncoder;
    struct ndn_TlvEncoder tlvEncoder;
    size_t signedPortionBeginOffset, signedPortionEndOffset;
    ndn_Error error;

    data.signature.keyLocator.type = ndn_KeyLocatorType_KEYNAME;
    data.signature.keyLocator.keyName = certificateName;
    data.signature.keyLocator.keyNameType = (ndn_KeyNameType)-1;
    data.signature.publisherPublicKeyDigest.publisherPublicKeyDigest = publisherPublicKeyDigest;
    if (useCrypto) {
      // Encode once to get the signed portion.
      ndn_DynamicUInt8Array_initialize(&output, encoding, maxEncodingLength, 0);
      if (WireFormat::getDefaultWireFormat() == BinaryXmlWireFormat::get()) {
        ndn_BinaryXmlEncoder_initialize(&binaryXmlEncoder, &output);    
        if ((error = ndn_encodeBinaryXmlData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &binaryXmlEncoder))) {
          cout << "Error in ndn_encodeBinaryXmlData: " << ndn_getErrorString(error) << endl;
          return 0;
        }
      }
      else {
        ndn_TlvEncoder_initialize(&tlvEncoder, &output);    
        if ((error = ndn_encodeTlvData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &tlvEncoder))) {
          cout << "Error in ndn_encodeTlvData: " << ndn_getErrorString(error) << endl;
          return 0;
        }
      }
      
      // Imitate MemoryPrivateKeyStorage::sign.
      uint8_t digest[SHA256_DIGEST_LENGTH];
      ndn_digestSha256(encoding + signedPortionBeginOffset, signedPortionEndOffset - signedPortionBeginOffset, digest);
      unsigned int signatureBitsLength;
      if (!RSA_sign(NID_sha256, digest, sizeof(digest), signatureBitsArray, &signatureBitsLength, privateKey)) {
        // Don't expect this to happen.
        cout << "Error in RSA_sign" << endl;
        return 0;
      }    
      
      ndn_Blob_initialize(&data.signature.signature, signatureBitsArray, signatureBitsLength);
    }
    else
      // Set up the signature, but don't sign.
      ndn_Blob_initialize(&data.signature.signature, signatureBitsArray, sizeof(signatureBitsArray));

    // Assume the encoding buffer is big enough so we don't need to dynamically reallocate.
    ndn_DynamicUInt8Array_initialize(&output, encoding, maxEncodingLength, 0);
    if (WireFormat::getDefaultWireFormat() == BinaryXmlWireFormat::get()) {
      ndn_BinaryXmlEncoder_initialize(&binaryXmlEncoder, &output);    
      if ((error = ndn_encodeBinaryXmlData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &binaryXmlEncoder))) {
        cout << "Error in ndn_encodeBinaryXmlData: " << ndn_getErrorString(error) << endl;
        return 0;
      }    
      *encodingLength = binaryXmlEncoder.offset;
    }
    else {
      ndn_TlvEncoder_initialize(&tlvEncoder, &output);    
      if ((error = ndn_encodeTlvData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &tlvEncoder))) {
        cout << "Error in ndn_encodeTlvData: " << ndn_getErrorString(error) << endl;
        return 0;
      }    
      *encodingLength = tlvEncoder.offset;
    }
  }
  double finish = getNowSeconds();
  
  if (privateKey)
    RSA_free(privateKey);
  
  return finish - start;
}

/**
 * Loop to decode a data packet nIterations times using C.
 * @param nIterations The number of iterations.
 * @param useCrypto If true, verify the signature.  If false, don't verify.
 * @param encoding The buffer with wire encoding to decode.
 * @param encodingLength The number of bytes in the encoding.
 * @return The number of seconds for all iterations.
 */
static double 
benchmarkDecodeDataSecondsC(int nIterations, bool useCrypto, uint8_t* encoding, size_t encodingLength)
{
  double start = getNowSeconds();
  for (int i = 0; i < nIterations; ++i) {
    struct ndn_NameComponent nameComponents[100];
    struct ndn_NameComponent keyNameComponents[100];
    struct ndn_Data data;
    ndn_Data_initialize
      (&data, nameComponents, sizeof(nameComponents) / sizeof(nameComponents[0]), 
       keyNameComponents, sizeof(keyNameComponents) / sizeof(keyNameComponents[0]));

    size_t signedPortionBeginOffset, signedPortionEndOffset;
    ndn_Error error;
    if (WireFormat::getDefaultWireFormat() == BinaryXmlWireFormat::get()) {
      ndn_BinaryXmlDecoder decoder;
      ndn_BinaryXmlDecoder_initialize(&decoder, encoding, encodingLength);  
      if ((error = ndn_decodeBinaryXmlData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &decoder))) {
        cout << "Error in ndn_decodeBinaryXmlData: " << ndn_getErrorString(error) << endl;
        return 0;
      }
    } 
    else {
      ndn_TlvDecoder decoder;
      ndn_TlvDecoder_initialize(&decoder, encoding, encodingLength);  
      if ((error = ndn_decodeTlvData(&data, &signedPortionBeginOffset, &signedPortionEndOffset, &decoder))) {
        cout << "Error in ndn_decodeTlvData: " << ndn_getErrorString(error) << endl;
        return 0;
      }
    }
    
    if (useCrypto) {
      if (!verifyRsaSignature
          (encoding + signedPortionBeginOffset, signedPortionEndOffset - signedPortionBeginOffset,
           data.signature.signature.value, data.signature.signature.length,
           DEFAULT_PUBLIC_KEY_DER, sizeof(DEFAULT_PUBLIC_KEY_DER)))
        cout << "Signature verification: FAILED" << endl;
    }
  }
  double finish = getNowSeconds();
 
  return finish - start;
}

/**
 * Call benchmarkEncodeDataSecondsCpp and benchmarkDecodeDataSecondsCpp with appropriate nInterations.  Print the 
 * results to cout.
 * @param useComplex See benchmarkEncodeDataSecondsCpp.
 * @param useCrypto See benchmarkEncodeDataSecondsCpp and benchmarkDecodeDataSecondsCpp.
 */
static void
benchmarkEncodeDecodeDataCpp(bool useComplex, bool useCrypto)
{
  const char *format = (WireFormat::getDefaultWireFormat() == BinaryXmlWireFormat::get() ? "ndnb" : "TLV ");
  Blob encoding;
  {
    int nIterations = useCrypto ? 20000 : 2000000;
    double duration = benchmarkEncodeDataSecondsCpp(nIterations, useComplex, useCrypto, encoding);
    cout << "Encode " << (useComplex ? "complex " : "simple  ") << format << " data C++: Crypto? " << (useCrypto ? "RSA" : "no ") 
         << ", Duration sec, Hz: " << duration << ", " << (nIterations / duration) << endl;  
  }
  {
    int nIterations = useCrypto ? 100000 : 2000000;
    double duration = benchmarkDecodeDataSecondsCpp(nIterations, useCrypto, encoding);
    cout << "Decode " << (useComplex ? "complex " : "simple  ") << format << " data C++: Crypto? " << (useCrypto ? "RSA" : "no ") 
         << ", Duration sec, Hz: " << duration << ", " << (nIterations / duration) << endl;  
  }
}

/**
 * Call benchmarkEncodeDataSecondsC and benchmarkDecodeDataSecondsC with appropriate nInterations.  Print the 
 * results to cout.
 * @param useComplex See benchmarkEncodeDataSecondsC.
 * @param useCrypto See benchmarkEncodeDataSecondsC and benchmarkDecodeDataSecondsC.
 */
static void
benchmarkEncodeDecodeDataC(bool useComplex, bool useCrypto)
{
  const char *format = (WireFormat::getDefaultWireFormat() == BinaryXmlWireFormat::get() ? "ndnb" : "TLV ");
  uint8_t encoding[1600];
  size_t encodingLength;
  {
    int nIterations = useCrypto ? 20000 : 10000000;
    double duration = benchmarkEncodeDataSecondsC(nIterations, useComplex, useCrypto, encoding, sizeof(encoding), &encodingLength);
    cout << "Encode " << (useComplex ? "complex " : "simple  ") << format << " data C:   Crypto? " << (useCrypto ? "RSA" : "no ") 
         << ", Duration sec, Hz: " << duration << ", " << (nIterations / duration) << endl;  
  }
  {
    int nIterations = useCrypto ? 150000 : 15000000;
    double duration = benchmarkDecodeDataSecondsC(nIterations, useCrypto, encoding, encodingLength);
    cout << "Decode " << (useComplex ? "complex " : "simple  ") << format << " data C:   Crypto? " << (useCrypto ? "RSA" : "no ") 
         << ", Duration sec, Hz: " << duration << ", " << (nIterations / duration) << endl;  
  }
}

int 
main(int argc, char** argv)
{
  try {
    // Make two passes, one for each wire format.
    for (int i = 1; i <= 2; ++i) {
      if (i == 1)
        WireFormat::setDefaultWireFormat(BinaryXmlWireFormat::get());
      else
        WireFormat::setDefaultWireFormat(TlvWireFormat::get());

      benchmarkEncodeDecodeDataCpp(false, false);
      benchmarkEncodeDecodeDataCpp(true, false);
      benchmarkEncodeDecodeDataCpp(false, true);
      benchmarkEncodeDecodeDataCpp(true, true);

      benchmarkEncodeDecodeDataC(false, false);
      benchmarkEncodeDecodeDataC(true, false);
      benchmarkEncodeDecodeDataC(false, true);
      benchmarkEncodeDecodeDataC(true, true);
    }
  } catch (std::exception& e) {
    cout << "exception: " << e.what() << endl;
  }
  return 0;
}
