//===========================================================================
// utils.cpp implements several utility functions 
//===========================================================================
#include <cmath>
#include <cstring>
#include <cassert>
#include <inttypes.h>
#include "utils.h"

string myDecStr(uint64_t v, uint32_t w)
{
   ostringstream o;
   o.width(w);
   o << v;
   string str(o.str());
   return str;
}

bool isPower2(uint32_t n)
{ return ((n & (n - 1)) == 0); }

int32_t floorLog2(uint32_t n)
{
   int32_t p = 0;

   if (n == 0) return -1;

   if (n & 0xffff0000) { p += 16; n >>= 16; }
   if (n & 0x0000ff00) { p +=  8; n >>=  8; }
   if (n & 0x000000f0) { p +=  4; n >>=  4; }
   if (n & 0x0000000c) { p +=  2; n >>=  2; }
   if (n & 0x00000002) { p +=  1; }

   return p;
}

int32_t ceilLog2(uint32_t n)
{ return floorLog2(n - 1) + 1; }

bool isPerfectSquare(uint32_t n)
{ 
   uint32_t sqrtn = (uint32_t) floor(sqrt((float) n));
   return (n == (sqrtn * sqrtn)) ? true : false;
}

// Is even and odd?
bool isEven(uint32_t n)
{ return ((n%2) == 0); }

bool isOdd(uint32_t n)
{ return !isEven(n); }

// Converts bits to bytes
uint32_t convertBitsToBytes(uint32_t num_bits)
{
   return ((num_bits % 8) == 0) ? (num_bits/8) : (num_bits/8 + 1);
}

// Trim the beginning and ending spaces in a string
string trimSpaces(string& str)
{
   size_t startpos = str.find_first_not_of(" \t");
   size_t endpos = str.find_last_not_of(" \t");

   if ((startpos == string::npos) && (endpos == string::npos))
   {
      return ("");
   }
   else
   {
      return (str.substr(startpos, endpos - startpos + 1));
   }
}

// Parse an arbitrary list separated by arbitrary delimiters
// into a vector of strings
void parseList(string& list, vector<string>& vec, string delim)
{
   list = trimSpaces(list);
   if (list == "")
      return;

   size_t i = 0;
   bool end_reached = false;

   if (delim.length() == 1)
   {
      char separator = delim[0];
      
      while(!end_reached)
      {
         size_t position = list.find(separator, i);
         string value;

         if (position != string::npos)
         {
            // The end of the string has not been reached
            value = list.substr(i, position-i);
         }
         else
         {
            // The end of the string has been reached
            value = list.substr(i);
            end_reached = true;
         }
         
         vec.push_back(value);

         i = position + 1;
      }
   }

   else if (delim.length() == 2)
   {
      char start_delim = delim[0];
      char end_delim = delim[1];

      while(!end_reached)
      {
         size_t start_position = list.find(start_delim, i);
         string value;

         if (start_position != string::npos)
         {
            // The end of the string has not been reached
            size_t end_position = list.find(end_delim, i);

            if (end_position == string::npos)
            {
               fprintf(stderr, "Parser Error: Wrong Format(%c%c)\n", start_delim, end_delim);
               exit(EXIT_FAILURE);
            }

            value = list.substr(start_position + 1, end_position - start_position - 1);
         
            vec.push_back(value);
            i = end_position + 1;
         }
         else
         {
            end_reached = true;
         }
      }
   }

   else
   {
      fprintf(stderr, "Unsupported Number of delimiters(%s)\n", delim.c_str());
      exit(EXIT_FAILURE);
   }
}

void splitIntoTokens(string line, vector<string>& tokens, const char* delimiters)
{
   char character_set[line.length() + 1];
   strncpy(character_set, line.c_str(), line.length() + 1);

   char* save_ptr;
   char* token = strtok_r(character_set, delimiters, &save_ptr);
   while (token != NULL)
   {
      tokens.push_back((string) token);
      token = strtok_r(NULL, delimiters, &save_ptr);
   }
}

double computeMean(const vector<uint64_t>& vec)
{
   double sigma_x = 0.0;
   for (vector<uint64_t>::const_iterator it = vec.begin(); it != vec.end(); it++)
   {
      uint64_t element = (*it);
      sigma_x += ((double) element);
   }

   size_t n = vec.size();
   assert(n>0);
   double mean = (sigma_x / n);
   return mean;
}

double computeStddev(const vector<uint64_t>& vec)
{
   double sigma_x = 0.0;
   double sigma_x2 = 0.0;
   for (vector<uint64_t>::const_iterator it = vec.begin(); it != vec.end(); it++)
   {
      uint64_t element = (*it);
      sigma_x += ((double) element);
      sigma_x2 += ((double) (element * element));
   }

   size_t n = vec.size();
   assert(n>0);
   double mean = (sigma_x / n);
   double variance = (sigma_x2 / n) - (mean * mean);
   assert(variance >= 0);
   double stddev = sqrt(variance);
   return stddev;
}

double computeCoefficientOfVariation(double mean, double stddev)
{
   return stddev / mean;
}
