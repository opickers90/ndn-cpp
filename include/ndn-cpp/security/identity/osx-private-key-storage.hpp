/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2013-2014 Regents of the University of California.
 * @author: Yingdi Yu <yingdi@cs.ucla.edu>
 * See COPYING for copyright and distribution information.
 */

#ifndef NDN_OSX_PRIVATEKEY_STORAGE_H
#define NDN_OSX_PRIVATEKEY_STORAGE_H

// Only compile if ndn-cpp-config.h defines NDN_CPP_HAVE_OSX_SECURITY 1.
#include <ndn-cpp/ndn-cpp-config.h>
#if NDN_CPP_HAVE_OSX_SECURITY

#include "../../common.hpp"
#include "private-key-storage.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <CoreServices/CoreServices.h>

namespace ndn
{


/**
 * CFReleaser is a helper class to wrap CoreFoundation object pointers.
 *
 * The class is similar in spirit to shared_ptr, but uses CoreFoundation
 * mechanisms to retain/release object.
 *
 * Original implementation by Christopher Hunt and it was borrowed from
 * http://www.cocoabuilder.com/archive/cocoa/130776-auto-cfrelease-and.html
 */
template<class T>
class CFReleaser
{
public:
  //////////////////////////////
  // Construction/destruction //

  CFReleaser()
    : m_typeRef(0)
  {
  }

  CFReleaser(const T& typeRef)
    : m_typeRef(typeRef)
  {
  }

  CFReleaser(const CFReleaser& inReleaser)
    : m_typeRef(0)
  {
    retain(inReleaser.m_typeRef);
  }

  CFReleaser&
  operator=(const T& typeRef)
  {
    if (typeRef != m_typeRef) {
      release();
      m_typeRef = typeRef;
    }
    return *this;
  }

  CFReleaser&
  operator=(const CFReleaser& inReleaser)
  {
    retain(inReleaser.m_typeRef);
    return *this;
  }

  ~CFReleaser()
  {
    release();
  }

  ////////////
  // Access //

  // operator const T&() const
  // {
  //   return m_typeRef;
  // }

  // operator T&()
  // {
  //   return m_typeRef;
  // }

  const T&
  get() const
  {
    return m_typeRef;
  }

  T&
  get()
  {
    return m_typeRef;
  }

  ///////////////////
  // Miscellaneous //

  void
  retain(const T& typeRef)
  {
    if (typeRef != 0) {
      CFRetain(typeRef);
    }
    release();
    m_typeRef = typeRef;
  }

  void release()
  {
    if (m_typeRef != 0) {
      CFRelease(m_typeRef);
      m_typeRef = 0;
    }
  };

private:
  T m_typeRef;
};
  
class OSXPrivateKeyStorage : public PrivateKeyStorage {
public:
  /**
   * Create a new OSXPrivateKeyStorage.
   */
  OSXPrivateKeyStorage() {}

  /**
   * destructor of OSXPrivateKeyStore
   */    
  virtual 
  ~OSXPrivateKeyStorage();

  /**
   * Generate a pair of asymmetric keys.
   * @param keyName The name of the key pair.
   * @param keyType The type of the key pair, e.g. KEY_TYPE_RSA.
   * @param keySize The size of the key pair.
   */
  virtual void 
  generateKeyPair(const Name& keyName, KeyType keyType = KEY_TYPE_RSA, int keySize = 2048);

  /**
   * Get the public key
   * @param keyName The name of public key.
   * @return The public key.
   */
  virtual ptr_lib::shared_ptr<PublicKey> 
  getPublicKey(const Name& keyName);
  
  /**
   * Fetch the private key for keyName and sign the data, returning a signature Blob.
   * @param data Pointer to the input byte array.
   * @param dataLength The length of data.
   * @param keyName The name of the signing key.
   * @param digestAlgorithm the digest algorithm.
   * @return The signature, or a null pointer if signing fails.
   */  
  virtual Blob 
  sign(const uint8_t *data, size_t dataLength, const Name& keyName, DigestAlgorithm digestAlgorithm = DIGEST_ALGORITHM_SHA256);
      
  /**
   * Decrypt data.
   * @param keyName The name of the decrypting key.
   * @param data The byte to be decrypted.
   * @param dataLength the length of data.
   * @param isSymmetric If true symmetric encryption is used, otherwise asymmetric decryption is used.
   * @return The decrypted data.
   */
  virtual Blob 
  decrypt(const Name& keyName, const uint8_t* data, size_t dataLength, bool isSymmetric = false);

  /**
   * Encrypt data.
   * @param keyName The name of the encrypting key.
   * @param data The byte to be encrypted.
   * @param dataLength the length of data.
   * @param isSymmetric If true symmetric encryption is used, otherwise asymmetric decryption is used.
   * @return The encrypted data.
   */
  virtual Blob
  encrypt(const Name& keyName, const uint8_t* data, size_t dataLength, bool isSymmetric = false);

  /**
   * Generate a symmetric key.
   * @param keyName The name of the key.
   * @param keyType The type of the key, e.g. KEY_TYPE_AES.
   * @param keySize The size of the key.
   */
  virtual void 
  generateKey(const Name& keyName, KeyType keyType = KEY_TYPE_AES, int keySize = 256);

  /**
   * Check if a particular key exists.
   * @param keyName The name of the key.
   * @param keyClass The class of the key, e.g. KEY_CLASS_PUBLIC, KEY_CLASS_PRIVATE, or KEY_CLASS_SYMMETRIC.
   * @return True if the key exists, otherwise false.
   */
  virtual bool
  doesKeyExist(const Name& keyName, KeyClass keyClass);  

  /**
   * configure ACL of a particular key
   * @param keyName the name of key
   * @param keyClass the class of key, e.g. Private Key
   * @param acl the new acl of the key
   * @param appPath the absolute path to the application
   * @returns true if setting succeeds
   */
  bool 
  setACL(const Name & keyName, KeyClass keyClass, int acl, const std::string & appPath);

 private:
  /**
   * convert NDN name of a key to internal name of the key
   * @param keyName the NDN name of the key
   * @param keyClass the class of the key
   * @return the internal key name
   */
  std::string 
  toInternalKeyName(const Name & keyName, KeyClass keyClass);

  /**
   * Get key
   * @param keyName the name of the key
   * @param keyClass the class of the key
   * @returns 0 if not found,  otherwise a Keychain ref to the key. You 
   * must use CFRelease to free it.
   */
  CFReleaser<SecKeychainItemRef> 
  getKey(const Name & keyName, KeyClass keyClass);
      
  /**
   * convert keyType to MAC OS symmetric key key type
   * @param keyType
   * @returns MAC OS key type
   */
  const CFTypeRef 
  getSymmetricKeyType(KeyType keyType);

  /**
   * convert keyType to MAC OS asymmetirc key type
   * @param keyType
   * @returns MAC OS key type
   */
  const CFTypeRef 
  getAsymmetricKeyType(KeyType keyType);

  /**
   * convert keyClass to MAC OS key class
   * @param keyClass
   * @returns MAC OS key class
   */
  const CFTypeRef 
  getKeyClass(KeyClass keyClass);

  /**
   * convert digestAlgorithm to MAC OS algorithm id
   * @param digestAlgorithm
   * @returns MAC OS algorithm id
   */
  const CFStringRef 
  getDigestAlgorithm(DigestAlgorithm digestAlgorithm);

  /**
   * get the digest size of the corresponding algorithm
   * @param digestAlgo the digest algorithm
   * @return digest size
   */
  long 
  getDigestSize(DigestAlgorithm digestAlgo);
};
  
}

#endif // NDN_CPP_HAVE_OSX_SECURITY

#endif
