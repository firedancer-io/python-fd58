/* Original source:
   https://github.com/firedancer-io/firedancer/blob/main/src/ballet/base58/fd_base58_tmpl.c */

/* Declares conversion functions to/from base58 for a specific size of
   binary data.

   To use this template, define:

     N: the length of the binary data (in bytes) to convert.  N must be
         32 or 64 in the current implementation.
     INTERMEDIATE_SZ: ceil(log_(58^5) ( (256^N) - 1)). In an ideal
         world, this could be computed from N, but there's no way the
         preprocessor can do math like that.
     BINARY_SIZE: N/4.  Define it yourself to facilitate declaring the
         required tables.

   INTERMEDIATE_SZ and BINARY_SZ should expand to uint64_ts while N should
   be an integer literal.

   Expects that enc_table_N, dec_table_N, and FD_BASE58_ENCODED_N_SZ
   exist (substituting the numeric value of N).

   This file is safe for inclusion multiple times. */

#define BYTE_CNT     ((uint64_t) N)
#define SUFFIX(s)    FD_EXPAND_THEN_CONCAT3(s,_,N)
#define ENCODED_SZ() FD_EXPAND_THEN_CONCAT3(FD_BASE58_ENCODED_, N, _SZ)
#define RAW58_SZ     (INTERMEDIATE_SZ*5UL)

#define INTERMEDIATE_SZ_W_PADDING INTERMEDIATE_SZ

char *
SUFFIX(fd_base58_encode)( unsigned char const * bytes,
                          unsigned            * opt_len,
                          char                * out ) {

  /* Count leading zeros (needed for final output) */

  uint64_t in_leading_0s = 0UL;
  for( ; in_leading_0s<BYTE_CNT; in_leading_0s++ ) if( bytes[ in_leading_0s ] ) break;

  /* X = sum_i bytes[i] * 2^(8*(BYTE_CNT-1-i)) */

  /* Convert N to 32-bit limbs:
     X = sum_i binary[i] * 2^(32*(BINARY_SZ-1-i)) */
  uint32_t binary[ BINARY_SZ ];
  for( uint64_t i=0UL; i<BINARY_SZ; i++ ) {
    uint32_t limb;
    memcpy( &limb, &bytes[ i*sizeof(uint32_t) ], sizeof(uint32_t) );
    binary[ i ] = __builtin_bswap32( limb );
  }

  uint64_t R1div = 656356768UL; /* = 58^5 */

  /* Convert to the intermediate format:
       X = sum_i intermediate[i] * 58^(5*(INTERMEDIATE_SZ-1-i))
     Initially, we don't require intermediate[i] < 58^5, but we do want
     to make sure the sums don't overflow. */

  uint64_t intermediate[ INTERMEDIATE_SZ_W_PADDING ];

  memset( intermediate, 0, INTERMEDIATE_SZ_W_PADDING * sizeof(uint64_t) );

# if N==32

  /* The worst case is if binary[7] is (2^32)-1. In that case
     intermediate[8] will be be just over 2^63, which is fine. */

  for( uint64_t i=0UL; i < BINARY_SZ; i++ )
    for( uint64_t j=0UL; j < INTERMEDIATE_SZ-1UL; j++ )
      intermediate[ j+1UL ] += (uint64_t)binary[ i ] * (uint64_t)SUFFIX(enc_table)[ i ][ j ];

# elif N==64

  /* If we do it the same way as the 32B conversion, intermediate[16]
     can overflow when the input is sufficiently large.  We'll do a
     mini-reduction after the first 8 steps.  After the first 8 terms,
     the largest intermediate[16] can be is 2^63.87.  Then, after
     reduction it'll be at most 58^5, and after adding the last terms,
     it won't exceed 2^63.1.  We do need to be cautious that the
     mini-reduction doesn't cause overflow in intermediate[15] though.
     Pre-mini-reduction, it's at most 2^63.05.  The mini-reduction adds
     at most 2^64/58^5, which is negligible.  With the final terms, it
     won't exceed 2^63.69, which is fine. Other terms are less than
     2^63.76, so no problems there. */

  for( uint64_t i=0UL; i < 8UL; i++ )
    for( uint64_t j=0UL; j < INTERMEDIATE_SZ-1UL; j++ )
      intermediate[ j+1UL ] += (uint64_t)binary[ i ] * (uint64_t)SUFFIX(enc_table)[ i ][ j ];
  /* Mini-reduction */
  intermediate[ 15 ] += intermediate[ 16 ]/R1div;
  intermediate[ 16 ] %= R1div;
  /* Finish iterations */
  for( uint64_t i=8UL; i < BINARY_SZ; i++ )
    for( uint64_t j=0UL; j < INTERMEDIATE_SZ-1UL; j++ )
      intermediate[ j+1UL ] += (uint64_t)binary[ i ] * (uint64_t)SUFFIX(enc_table)[ i ][ j ];

# else
# error "Add support for this N"
# endif

  /* Now we make sure each term is less than 58^5. Again, we have to be
     a bit careful of overflow.

     For N==32, in the worst case, as before, intermediate[8] will be
     just over 2^63 and intermediate[7] will be just over 2^62.6.  In
     the first step, we'll add floor(intermediate[8]/58^5) to
     intermediate[7].  58^5 is pretty big though, so intermediate[7]
     barely budges, and this is still fine.

     For N==64, in the worst case, the biggest entry in intermediate at
     this point is 2^63.87, and in the worst case, we add (2^64-1)/58^5,
     which is still about 2^63.87. */

  for( uint64_t i=INTERMEDIATE_SZ-1UL; i>0UL; i-- ) {
    intermediate[ i-1UL ] += (intermediate[ i ]/R1div);
    intermediate[ i     ] %= R1div;
  }

  /* Convert intermediate form to base 58.  This form of conversion
     exposes tons of ILP, but it's more than the CPU can take advantage
     of.
       X = sum_i raw_base58[i] * 58^(RAW58_SZ-1-i) */

  uint8_t raw_base58[ RAW58_SZ ];
  for( uint64_t i=0UL; i<INTERMEDIATE_SZ; i++) {
    /* We know intermediate[ i ] < 58^5 < 2^32 for all i, so casting to
       a uint32_t is safe.  GCC doesn't seem to be able to realize this, so
       when it converts uint64_t/uint64_t to a magic multiplication, it
       generates the single-op 64b x 64b -> 128b mul instruction.  This
       hurts the CPU's ability to take advantage of the ILP here. */
    uint32_t v = (uint32_t)intermediate[ i ];
    raw_base58[ 5UL*i+4UL ] = (uint8_t)((v/1U       )%58U);
    raw_base58[ 5UL*i+3UL ] = (uint8_t)((v/58U      )%58U);
    raw_base58[ 5UL*i+2UL ] = (uint8_t)((v/3364U    )%58U);
    raw_base58[ 5UL*i+1UL ] = (uint8_t)((v/195112U  )%58U);
    raw_base58[ 5UL*i+0UL ] = (uint8_t)( v/11316496U); /* We know this one is less than 58 */
  }

  /* Finally, actually convert to the string.  We have to ignore all the
     leading zeros in raw_base58 and instead insert in_leading_0s
     leading '1' characters.  We can show that raw_base58 actually has
     at least in_leading_0s, so we'll do this by skipping the first few
     leading zeros in raw_base58. */

  uint64_t raw_leading_0s = 0UL;
  for( ; raw_leading_0s<RAW58_SZ; raw_leading_0s++ ) if( raw_base58[ raw_leading_0s ] ) break;

  /* It's not immediately obvious that raw_leading_0s >= in_leading_0s,
     but it's true.  In base b, X has floor(log_b X)+1 digits.  That
     means in_leading_0s = N-1-floor(log_256 X) and raw_leading_0s =
     RAW58_SZ-1-floor(log_58 X).  Let X<256^N be given and consider:

     raw_leading_0s - in_leading_0s =
       =  RAW58_SZ-N + floor( log_256 X ) - floor( log_58 X )
       >= RAW58_SZ-N - 1 + ( log_256 X - log_58 X ) .

     log_256 X - log_58 X is monotonically decreasing for X>0, so it
     achieves it minimum at the maximum possible value for X, i.e.
     256^N-1.
       >= RAW58_SZ-N-1 + log_256(256^N-1) - log_58(256^N-1)

     When N==32, RAW58_SZ is 45, so this gives skip >= 0.29
     When N==64, RAW58_SZ is 90, so this gives skip >= 1.59.

     Regardless, raw_leading_0s - in_leading_0s >= 0. */

  uint64_t skip = raw_leading_0s - in_leading_0s;
  for( uint64_t i=0UL; i<RAW58_SZ-skip; i++ )  out[ i ] = base58_chars[ raw_base58[ skip+i ] ];

  out[ RAW58_SZ-skip ] = '\0';
  if( opt_len ) *opt_len = (unsigned)(RAW58_SZ-skip);
  return out;
}

uint8_t *
SUFFIX(fd_base58_decode)( char const *    encoded,
                          unsigned char * out ) {

  /* Validate string and count characters before the nul terminator */

  uint64_t char_cnt = 0UL;
  for( ; char_cnt<ENCODED_SZ(); char_cnt++ ) {
    char c = encoded[ char_cnt ];
    if( !c ) break;
    /* If c<'1', this will underflow and idx will be huge */
    uint64_t idx = (uint64_t)(uint8_t)c - (uint64_t)BASE58_INVERSE_TABLE_OFFSET;
    idx = idx<BASE58_INVERSE_TABLE_SENTINEL ? idx : BASE58_INVERSE_TABLE_SENTINEL;
    if( FD_UNLIKELY( base58_inverse[ idx ] == BASE58_INVALID_CHAR ) ) return NULL;
  }

  if( FD_UNLIKELY( char_cnt == ENCODED_SZ() ) ) return NULL; /* too long */

  /* X = sum_i raw_base58[i] * 58^(RAW58_SZ-1-i) */

  uint8_t raw_base58[ RAW58_SZ ];

  /* Prepend enough 0s to make it exactly RAW58_SZ characters */

  uint64_t prepend_0 = RAW58_SZ-char_cnt;
  for( uint64_t j=0UL; j<RAW58_SZ; j++ )
    raw_base58[ j ] = (j<prepend_0) ? (uint8_t)0 : base58_inverse[ encoded[ j-prepend_0 ] - BASE58_INVERSE_TABLE_OFFSET ];

  /* Convert to the intermediate format (base 58^5):
       X = sum_i intermediate[i] * 58^(5*(INTERMEDIATE_SZ-1-i)) */

  uint64_t intermediate[ INTERMEDIATE_SZ ];
  for( uint64_t i=0UL; i<INTERMEDIATE_SZ; i++ )
    intermediate[ i ] = (uint64_t)raw_base58[ 5UL*i+0UL ] * 11316496UL +
                        (uint64_t)raw_base58[ 5UL*i+1UL ] * 195112UL   +
                        (uint64_t)raw_base58[ 5UL*i+2UL ] * 3364UL     +
                        (uint64_t)raw_base58[ 5UL*i+3UL ] * 58UL       +
                        (uint64_t)raw_base58[ 5UL*i+4UL ] * 1UL;


  /* Using the table, convert to overcomplete base 2^32 (terms can be
     larger than 2^32).  We need to be careful about overflow.

     For N==32, the largest anything in binary can get is binary[7]:
     even if intermediate[i]==58^5-1 for all i, then binary[7] < 2^63.

     For N==64, the largest anything in binary can get is binary[13]:
     even if intermediate[i]==58^5-1 for all i, then binary[13] <
     2^63.998.  Hanging in there, just by a thread! */

  uint64_t binary[ BINARY_SZ ];
  for( uint64_t j=0UL; j<BINARY_SZ; j++ ) {
    uint64_t acc=0UL;
    for( uint64_t i=0UL; i<INTERMEDIATE_SZ; i++ )
      acc += (uint64_t)intermediate[ i ] * (uint64_t)SUFFIX(dec_table)[ i ][ j ];
    binary[ j ] = acc;
  }

  /* Make sure each term is less than 2^32.

     For N==32, we have plenty of headroom in binary, so overflow is
     not a concern this time.

     For N==64, even if we add 2^32 to binary[13], it is still 2^63.998,
     so this won't overflow. */

  for( uint64_t i=BINARY_SZ-1UL; i>0UL; i-- ) {
    binary[ i-1UL ] += (binary[i] >> 32);
    binary[ i     ] &= 0xFFFFFFFFUL;
  }

  /* If the largest term is 2^32 or bigger, it means N is larger than
     what can fit in BYTE_CNT bytes.  This can be triggered, by passing
     a base58 string of all 'z's for example. */

  if( FD_UNLIKELY( binary[ 0UL ] > 0xFFFFFFFFUL ) ) return NULL;

  /* Convert each term to big endian for the final output */

  uint32_t * out_as_uint = (uint32_t*)out;
  for( uint64_t i=0UL; i<BINARY_SZ; i++ ) {
    out_as_uint[ i ] = __builtin_bswap32( (uint32_t)binary[ i ] );
  }
  /* Make sure the encoded version has the same number of leading '1's
     as the decoded version has leading 0s. The check doesn't read past
     the end of encoded, because '\0' != '1', so it will return NULL. */

  uint64_t leading_zero_cnt = 0UL;
  for( ; leading_zero_cnt<BYTE_CNT; leading_zero_cnt++ ) {
    if( out[ leading_zero_cnt ] ) break;
    if( FD_UNLIKELY( encoded[ leading_zero_cnt ] != '1' ) ) return NULL;
  }
  if( FD_UNLIKELY( encoded[ leading_zero_cnt ] == '1' ) ) return NULL;
  return out;
}

#undef RAW58_SZ
#undef ENCODED_SZ
#undef SUFFIX

#undef BINARY_SZ
#undef BYTE_CNT
#undef INTERMEDIATE_SZ
#undef N