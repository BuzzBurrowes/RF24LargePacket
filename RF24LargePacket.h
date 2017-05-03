#include <RF24.h>

template <int S>
class RF24LargePacket : public RF24
{
public:
   RF24LargePacket(uint8_t _cepin, uint8_t _cspin)
      : RF24(_cepin, _cspin)
   {}

   RF24LargePacket(uint8_t _cepin, uint8_t _cspin, uint32_t spispeed)
      : RF24(_cepin, _cspin, spispeed)
   {}

   virtual uint16_t getDynamicPayloadSize(void) override
   {
      if (mBytesRemaining == 0)
         return mReceivedBytes;
      return 0;
   }

   virtual bool available(void) override
   {
      if (mBytesRemaining == 0 && mReceivedBytes > 0) return true;

      if (!RF24::available())
      {
         return false;
      }
      
      IF_SERIAL_DEBUG(printf_P(PSTR("packet available...\n")));

      // base has data, so let's get it...
      static char packet[32];
      RF24::read(packet, 32);

      if (!mBytesRemaining)
      {
         IF_SERIAL_DEBUG(printf_P(PSTR("Expecting a header packet... "))/*; _DumpPacket(packet)*/);
         HeaderPacket& hp = *((HeaderPacket*)packet);
         if (hp.magicNumber != magicNumber || hp.fullPayloadSize > S)
         {
            IF_SERIAL_DEBUG(printf_P(PSTR("Invalid Packet Header!\n")));
            return false;
         }
         mBytesRemaining = hp.fullPayloadSize;
         uint8_t size = (mBytesRemaining > 29) ? 29 : mBytesRemaining;
         IF_SERIAL_DEBUG(printf_P(PSTR("valid.\nExpecting %d total bytes.\nCopying %d bytes."), hp.fullPayloadSize, size)/*; _DumpPacket(packet)*/);
         memcpy(mRecvBuffer, hp.data, size);
         mBytesRemaining -= size;
         mReceivedBytes = size;
      }
      else
      {
         // this is a stream packet...
         uint8_t expectedIndex = ((mReceivedBytes - 29) / 31) + 1;
         IF_SERIAL_DEBUG(printf_P(PSTR("Expecting stream packet %d... "), expectedIndex)/*; _DumpPacket(packet)*/);
         StreamPacket& sp = *((StreamPacket*)packet);
         if (sp.packetIndex != expectedIndex)
         {
            IF_SERIAL_DEBUG(printf_P(PSTR("Bad packet index!\nGot %d, expected %d having received %d bytes.\n"),sp.packetIndex,
                  expectedIndex, mReceivedBytes));
            mBytesRemaining = 0; 
            mReceivedBytes = 0;
            return false;
         }
         int  size = (mBytesRemaining > 31) ? 31 : mBytesRemaining;
         IF_SERIAL_DEBUG(printf_P(PSTR("valid.\nCopying %d bytes."), size)/*; _DumpPacket(packet)*/);
         uint8_t* dest = mRecvBuffer + mReceivedBytes;
         memcpy(dest, sp.data, size);
         mBytesRemaining -= size;
         mReceivedBytes += size;
      }
      IF_SERIAL_DEBUG(printf_P(PSTR("(%d bytes remaining)\n"), mBytesRemaining));

      return (mReceivedBytes > 0 && mBytesRemaining == 0);
   }

   virtual void read( void* buf, uint16_t len ) override
   {
      IF_SERIAL_DEBUG(printf_P(PSTR("READ: Copying %d bytes to output.\n"),mReceivedBytes)/*; _DumpPacket(mRecvBuffer, mReceivedBytes)*/);
      memcpy(buf, mRecvBuffer, mReceivedBytes);
      mReceivedBytes = 0;
      mBytesRemaining = 0;
   }

   virtual bool write( const void* buf, uint16_t len ) override
   {
      static char packet[32];
      uint8_t* srcBuffer = (uint8_t*)buf;
      HeaderPacket& hp = *((HeaderPacket*)packet);
      hp.magicNumber = magicNumber;
      hp.fullPayloadSize = len;
      int size = (len > 29) ? 29 : len;
      memcpy(hp.data, srcBuffer, size);
      IF_SERIAL_DEBUG(printf_P(PSTR("Writing header packet... "))/*; _DumpPacket(packet)*/);
      if (!RF24::write(packet,size + 2))
      {
         IF_SERIAL_DEBUG(printf_P(PSTR("Failed!\n")));
         return false;
      }
      IF_SERIAL_DEBUG(printf_P(PSTR("Done.\n")));
      srcBuffer += size;
      len -= size;
      StreamPacket& sp = *((StreamPacket*)packet);
      sp.packetIndex = 1;
      while (len)
      {
         int size = (len > 30) ? 31 : len;
         memcpy(sp.data, srcBuffer, size);
         IF_SERIAL_DEBUG(printf_P(PSTR("Writing stream packet %d... "), sp.packetIndex)/*; _DumpPacket(packet)*/);
         if (!RF24::write(packet, size + 1)) 
         {
            IF_SERIAL_DEBUG(printf_P(PSTR("Failed!\n")));
            return false;
         }
         IF_SERIAL_DEBUG(printf_P(PSTR("Done.\n")));
         srcBuffer += size;
         len -= size;
         sp.packetIndex++;
      }
      return true; 
   }

protected:
private:
   char    mRecvBuffer[S];
   uint16_t mLastPayloadReceivedSize {0};
   uint16_t mBytesRemaining {0};
   uint16_t mReceivedBytes  {0};

   static const uint8_t magicNumber = 0b11011011;

   #pragma pack(push,1)
   struct HeaderPacket
   {
      uint16_t fullPayloadSize;
      uint8_t  magicNumber {magicNumber};
      uint8_t  data[29];
   };
   struct StreamPacket
   {
      uint8_t packetIndex;
      uint8_t data[31];
   };
   #pragma pack(pop)

#ifdef SERIAL_DEBUG
   void _DumpPacket(uint8_t* packet, int len = 32)
   {
      for (int i = 0; i < 1000; i++)
      {
         for (int j = 0; j < 8; j++)
         {
            if ( ((i*8) + j) >= len) 
               return;
            Serial.print(packet[(i * 8) + j],HEX);
            Serial.print(" ");
         }
         Serial.println(" ");
      }
   }
#endif
};