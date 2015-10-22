/*
 The ASTEntryList class is utilized for storing information about multiple
 entries in an AST.  It allows for loading from/storing to a JSON representation.
*/

#ifndef AST_ENTRY_LIST_HPP
#define AST_ENTRY_LIST_HPP

#include "ASTEntry.hpp"

#include "third-party/picojson-1.3.0/picojson.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

namespace clang_mutate
{
  class ASTEntryList
  {
  private:
    std::vector<ASTEntry*> m_astEntries;

    void initFromJSONStream( std::istream& in )
    {
      if ( in.good() )
      {
        picojson::value json;
        in >> json;
        
        if ( picojson::get_last_error().empty() )
        {
          initFromJSON( json );
        }              
      }
    }
    void initFromJSON( const picojson::value &json )
    {
      if ( json.is<picojson::array>() )
      {
        picojson::array array = json.get<picojson::array>();
    
        for ( picojson::array::const_iterator iter = array.begin();
              iter != array.end();
              iter++ )
        {
          if ( ASTBinaryEntry::jsonObjHasRequiredFields( *iter ) )
          {
            m_astEntries.push_back( new ASTBinaryEntry( *iter ) );
          }
          else if ( ASTNonBinaryEntry::jsonObjHasRequiredFields( *iter ) )
          {
            m_astEntries.push_back( new ASTNonBinaryEntry( *iter ) );
          }
        }
      } 
    }
  public:
    ASTEntryList() {}
    ASTEntryList( const std::vector<ASTEntry*> &astEntries )
    {
      for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
            iter != m_astEntries.end();
            iter++ )
      {
        m_astEntries.push_back( (*iter)->clone() );
      }
    }

    ASTEntryList( const picojson::value &json )
    {
      initFromJSON( json );
    }

    ASTEntryList( std::istream &in )
    {
      initFromJSONStream( in );
    }

    ASTEntryList( const std::string &infilePath )
    {
      std::ifstream in( infilePath.c_str() );
      initFromJSONStream( in );
    }

    ~ASTEntryList() 
    {
      for ( unsigned i = 0; i < m_astEntries.size(); i++ )
      {
        delete m_astEntries[i];
      }
    }

    void addEntry(ASTEntry *astEntry)
    {
      m_astEntries.push_back(astEntry);
    }

    ASTEntry* getEntry(unsigned int counter)
    {
      for ( std::vector<ASTEntry*>::iterator iter = m_astEntries.begin();
            iter != m_astEntries.end();
            iter++ )
      {
        if ( (*iter)->getCounter() == counter )
        {
          return *iter;
        }
      }

      return NULL;
    } 

    std::string toString() const 
    {
      std::stringstream ret;

      for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
            iter != m_astEntries.end();
            iter++ )    
      {
        ret << (*iter)->toString() << std::endl;        
      }

      return ret.str();
    }

    picojson::value toJSON() const
    {
      picojson::array array;

      for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
            iter != m_astEntries.end();
            iter++ )
      {
        array.push_back( (*iter)->toJSON() );
      }

      return picojson::value(array);
    } 

    std::ostream& toStream(std::ostream& out )
    {
      for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
            iter != m_astEntries.end();
            iter++ )
      {
        out << (*iter)->toString() << std::endl;
      }

      return out;
    }

    llvm::raw_ostream& toStream(llvm::raw_ostream& out)
    {
      std::stringstream ss;
      toStream(ss);
     
      out << ss.str(); 
      return out;
    }

    std::ostream& toStreamJSON(std::ostream& out )
    {
      out << toJSON() << std::endl;
      return out;
    }

    llvm::raw_ostream& toStreamJSON(llvm::raw_ostream& out)
    {
      std::stringstream ss;
      toStreamJSON(ss);

      out << ss.str();
      return out;
    }

    bool toFile(const std::string& outfilePath )
    {
      std::ofstream outfile( outfilePath.c_str() );
      return toStream(outfile).good();
    }

    bool toFileJSON(const std::string& outfilePath )
    {
      std::ofstream outfile( outfilePath.c_str() );
      return toStreamJSON(outfile).good();
    }
  };
}

#endif
