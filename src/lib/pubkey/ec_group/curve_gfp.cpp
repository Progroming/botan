/*
* Elliptic curves over GF(p) Montgomery Representation
* (C) 2014,2015,2018 Jack Lloyd
*     2016 Matthias Gierlings
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/curve_gfp.h>
#include <botan/curve_nistp.h>
#include <botan/numthry.h>
#include <botan/reducer.h>
#include <botan/internal/mp_core.h>
#include <botan/internal/mp_asmi.h>

namespace Botan {

namespace {

class CurveGFp_Montgomery final : public CurveGFp_Repr
   {
   public:
      CurveGFp_Montgomery(const BigInt& p, const BigInt& a, const BigInt& b) :
         m_p(p), m_a(a), m_b(b),
         m_p_words(m_p.sig_words()),
         m_p_dash(monty_inverse(m_p.word_at(0)))
         {
         Modular_Reducer mod_p(m_p);

         m_r.set_bit(m_p_words * BOTAN_MP_WORD_BITS);
         m_r = mod_p.reduce(m_r);

         m_r2  = mod_p.square(m_r);
         m_r3  = mod_p.multiply(m_r, m_r2);
         m_a_r = mod_p.multiply(m_r, m_a);
         m_b_r = mod_p.multiply(m_r, m_b);
         }

      const BigInt& get_a() const override { return m_a; }

      const BigInt& get_b() const override { return m_b; }

      const BigInt& get_p() const override { return m_p; }

      const BigInt& get_a_rep() const override { return m_a_r; }

      const BigInt& get_b_rep() const override { return m_b_r; }

      bool is_one(const BigInt& x) const override { return x == m_r; }

      size_t get_p_words() const override { return m_p_words; }

      size_t get_ws_size() const override { return 2*m_p_words + 4; }

      BigInt invert_element(const BigInt& x, secure_vector<word>& ws) const override;

      void to_curve_rep(BigInt& x, secure_vector<word>& ws) const override;

      void from_curve_rep(BigInt& x, secure_vector<word>& ws) const override;

      void curve_mul(BigInt& z, const BigInt& x, const BigInt& y,
                     secure_vector<word>& ws) const override;

      void curve_mul_words(BigInt& z,
                           const word x_words[],
                           const size_t x_size,
                           const BigInt& y,
                           secure_vector<word>& ws) const override;

      void curve_sqr(BigInt& z, const BigInt& x,
                     secure_vector<word>& ws) const override;
   private:
      BigInt m_p;
      BigInt m_a, m_b;
      BigInt m_a_r, m_b_r;
      size_t m_p_words; // cache of m_p.sig_words()

      // Montgomery parameters
      BigInt m_r, m_r2, m_r3;
      word m_p_dash;
   };

BigInt CurveGFp_Montgomery::invert_element(const BigInt& x, secure_vector<word>& ws) const
   {
   // Should we use Montgomery inverse instead?
   const BigInt inv = inverse_mod(x, m_p);
   BigInt res;
   curve_mul(res, inv, m_r3, ws);
   return res;
   }

void CurveGFp_Montgomery::to_curve_rep(BigInt& x, secure_vector<word>& ws) const
   {
   const BigInt tx = x;
   curve_mul(x, tx, m_r2, ws);
   }

void CurveGFp_Montgomery::from_curve_rep(BigInt& z, secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   bigint_monty_redc(z.mutable_data(),
                     m_p.data(), m_p_words, m_p_dash,
                     ws.data(), ws.size());
   }

void CurveGFp_Montgomery::curve_mul(BigInt& z, const BigInt& x, const BigInt& y,
                                    secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   BOTAN_DEBUG_ASSERT(x.sig_words() <= m_p_words);
   BOTAN_DEBUG_ASSERT(y.sig_words() <= m_p_words);

   const size_t x_words = (x.size() >= m_p_words) ? m_p_words : x.sig_words();
   const size_t y_words = (y.size() >= m_p_words) ? m_p_words : y.sig_words();

   bigint_mul(z.mutable_data(), z.size(),
              x.data(), x.size(), x_words,
              y.data(), y.size(), y_words,
              ws.data(), ws.size());

   bigint_monty_redc(z.mutable_data(),
                     m_p.data(), m_p_words, m_p_dash,
                     ws.data(), ws.size());
   }

void CurveGFp_Montgomery::curve_mul_words(BigInt& z,
                                          const word x_w[],
                                          size_t x_size,
                                          const BigInt& y,
                                          secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   BOTAN_DEBUG_ASSERT(y.sig_words() <= m_p_words);

   const size_t x_words = (x_size >= m_p_words) ? m_p_words : x_size;
   const size_t y_words = (y.size() >= m_p_words) ? m_p_words : y.sig_words();

   bigint_mul(z.mutable_data(), z.size(),
              x_w, x_size, x_words,
              y.data(), y.size(), y_words,
              ws.data(), ws.size());

   bigint_monty_redc(z.mutable_data(),
                     m_p.data(), m_p_words, m_p_dash,
                     ws.data(), ws.size());
   }

void CurveGFp_Montgomery::curve_sqr(BigInt& z, const BigInt& x,
                                    secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   const size_t x_words = (x.size() >= m_p_words) ? m_p_words : x.sig_words();

   bigint_sqr(z.mutable_data(), z.size(),
              x.data(), x.size(), x_words,
              ws.data(), ws.size());

   bigint_monty_redc(z.mutable_data(),
                     m_p.data(), m_p_words, m_p_dash,
                     ws.data(), ws.size());
   }

class CurveGFp_NIST : public CurveGFp_Repr
   {
   public:
      CurveGFp_NIST(size_t p_bits, const BigInt& a, const BigInt& b) :
         m_a(a), m_b(b), m_p_words((p_bits + BOTAN_MP_WORD_BITS - 1) / BOTAN_MP_WORD_BITS)
         {
         }

      const BigInt& get_a() const override { return m_a; }

      const BigInt& get_b() const override { return m_b; }

      size_t get_p_words() const override { return m_p_words; }

      size_t get_ws_size() const override { return 2*m_p_words + 4; }

      const BigInt& get_a_rep() const override { return m_a; }

      const BigInt& get_b_rep() const override { return m_b; }

      bool is_one(const BigInt& x) const override { return x == 1; }

      void to_curve_rep(BigInt& x, secure_vector<word>& ws) const override
         { redc(x, ws); }

      void from_curve_rep(BigInt& x, secure_vector<word>& ws) const override
         { redc(x, ws); }

      BigInt invert_element(const BigInt& x, secure_vector<word>& ws) const override;

      void curve_mul(BigInt& z, const BigInt& x, const BigInt& y,
                     secure_vector<word>& ws) const override;

      void curve_mul_words(BigInt& z,
                           const word x_words[],
                           const size_t x_size,
                           const BigInt& y,
                           secure_vector<word>& ws) const override;

      void curve_sqr(BigInt& z, const BigInt& x,
                     secure_vector<word>& ws) const override;
   private:
      virtual void redc(BigInt& x, secure_vector<word>& ws) const = 0;

      // Curve parameters
      BigInt m_a, m_b;
      size_t m_p_words; // cache of m_p.sig_words()
   };


BigInt CurveGFp_NIST::invert_element(const BigInt& x, secure_vector<word>& ws) const
   {
   BOTAN_UNUSED(ws);
   return inverse_mod(x, get_p());
   }

void CurveGFp_NIST::curve_mul(BigInt& z, const BigInt& x, const BigInt& y,
                              secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   const size_t x_words = (x.size() >= m_p_words) ? m_p_words : x.sig_words();
   const size_t y_words = (y.size() >= m_p_words) ? m_p_words : y.sig_words();

   bigint_mul(z.mutable_data(), z.size(),
              x.data(), x.size(), x_words,
              y.data(), y.size(), y_words,
              ws.data(), ws.size());

   this->redc(z, ws);
   }

void CurveGFp_NIST::curve_mul_words(BigInt& z,
                                    const word x_w[],
                                    size_t x_size,
                                    const BigInt& y,
                                    secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   const size_t x_words = (x_size >= m_p_words) ? m_p_words : x_size;
   const size_t y_words = (y.size() >= m_p_words) ? m_p_words : y.sig_words();

   bigint_mul(z.mutable_data(), z.size(),
              x_w, x_size, x_words,
              y.data(), y.size(), y_words,
              ws.data(), ws.size());

   this->redc(z, ws);
   }

void CurveGFp_NIST::curve_sqr(BigInt& z, const BigInt& x,
                              secure_vector<word>& ws) const
   {
   if(ws.size() < get_ws_size())
      ws.resize(get_ws_size());

   const size_t output_size = 2*m_p_words + 2;
   if(z.size() < output_size)
      z.grow_to(output_size);

   const size_t x_words = (x.size() >= m_p_words) ? m_p_words : x.sig_words();

   bigint_sqr(z.mutable_data(), output_size,
              x.data(), x.size(), x_words,
              ws.data(), ws.size());

   this->redc(z, ws);
   }

#if defined(BOTAN_HAS_NIST_PRIME_REDUCERS_W32)

/**
* The NIST P-192 curve
*/
class CurveGFp_P192 final : public CurveGFp_NIST
   {
   public:
      CurveGFp_P192(const BigInt& a, const BigInt& b) : CurveGFp_NIST(192, a, b) {}
      const BigInt& get_p() const override { return prime_p192(); }
   private:
      void redc(BigInt& x, secure_vector<word>& ws) const override { redc_p192(x, ws); }
   };

/**
* The NIST P-224 curve
*/
class CurveGFp_P224 final : public CurveGFp_NIST
   {
   public:
      CurveGFp_P224(const BigInt& a, const BigInt& b) : CurveGFp_NIST(224, a, b) {}
      const BigInt& get_p() const override { return prime_p224(); }
   private:
      void redc(BigInt& x, secure_vector<word>& ws) const override { redc_p224(x, ws); }
   };

/**
* The NIST P-256 curve
*/
class CurveGFp_P256 final : public CurveGFp_NIST
   {
   public:
      CurveGFp_P256(const BigInt& a, const BigInt& b) : CurveGFp_NIST(256, a, b) {}
      const BigInt& get_p() const override { return prime_p256(); }
   private:
      void redc(BigInt& x, secure_vector<word>& ws) const override { redc_p256(x, ws); }
   };

/**
* The NIST P-384 curve
*/
class CurveGFp_P384 final : public CurveGFp_NIST
   {
   public:
      CurveGFp_P384(const BigInt& a, const BigInt& b) : CurveGFp_NIST(384, a, b) {}
      const BigInt& get_p() const override { return prime_p384(); }
   private:
      void redc(BigInt& x, secure_vector<word>& ws) const override { redc_p384(x, ws); }
   };

#endif

/**
* The NIST P-521 curve
*/
class CurveGFp_P521 final : public CurveGFp_NIST
   {
   public:
      CurveGFp_P521(const BigInt& a, const BigInt& b) : CurveGFp_NIST(521, a, b) {}
      const BigInt& get_p() const override { return prime_p521(); }
   private:
      void redc(BigInt& x, secure_vector<word>& ws) const override { redc_p521(x, ws); }
   };

}

std::shared_ptr<CurveGFp_Repr>
CurveGFp::choose_repr(const BigInt& p, const BigInt& a, const BigInt& b)
   {
#if defined(BOTAN_HAS_NIST_PRIME_REDUCERS_W32)
   if(p == prime_p192())
      return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_P192(a, b));
   if(p == prime_p224())
      return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_P224(a, b));
   if(p == prime_p256())
      return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_P256(a, b));
   if(p == prime_p384())
      return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_P384(a, b));
#endif

   if(p == prime_p521())
      return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_P521(a, b));

   return std::shared_ptr<CurveGFp_Repr>(new CurveGFp_Montgomery(p, a, b));
   }

}
