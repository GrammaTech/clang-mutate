/*
 See ASTEntryList.h
*/

#include "ASTEntryList.h"
#include "TypeDBEntry.h"

#include "Json.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>
#include <string>
#include <sstream>

namespace clang_mutate
{
  void ASTEntryList::initFromJSONStream( std::istream& in )
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

  void ASTEntryList::initFromJSON( const picojson::value &json )
  {
    if ( json.is<picojson::array>() )
    {
      picojson::array array = json.get<picojson::array>();
  
      for ( picojson::array::const_iterator iter = array.begin();
            iter != array.end();
            iter++ )
      {
        ASTEntry* astEntry = ASTEntryFactory::make( json );

        if ( astEntry != NULL )
          m_astEntries.push_back(astEntry);
      }
    } 
  }
  
  ASTEntryList::ASTEntryList() {}
  ASTEntryList::ASTEntryList( const std::vector<ASTEntry*> &astEntries )
  {
    for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
          iter != m_astEntries.end();
          iter++ )
    {
      m_astEntries.push_back( (*iter)->clone() );
    }
  }

  ASTEntryList::ASTEntryList( const picojson::value &json )
  {
    initFromJSON( json );
  }

  ASTEntryList::ASTEntryList( std::istream &in )
  {
    initFromJSONStream( in );
  }

  ASTEntryList::ASTEntryList( const std::string &infilePath )
  {
    std::ifstream in( infilePath.c_str() );
    initFromJSONStream( in );
  }

  ASTEntryList::~ASTEntryList() 
  {
    for ( unsigned i = 0; i < m_astEntries.size(); i++ )
    {
      delete m_astEntries[i];
    }
  }

  void ASTEntryList::addEntry(ASTEntry *astEntry)
  {
    m_astEntries.push_back(astEntry);
  }

  ASTEntry* ASTEntryList::getEntry(unsigned int counter)
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

  bool ASTEntryList::isEmpty() const { 
    return m_astEntries.size() == 0;
  }

  std::string ASTEntryList::toString() const 
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

  picojson::value ASTEntryList::toJSON() const
  {
    picojson::array array = TypeDBEntry::databaseToJSON();

    for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
          iter != m_astEntries.end();
          iter++ )
    {
      array.push_back( (*iter)->toJSON() );
    }

    return picojson::value(array);
  } 

  std::ostream& ASTEntryList::toStream(std::ostream& out )
  {
    for ( std::vector<ASTEntry*>::const_iterator iter = m_astEntries.begin();
          iter != m_astEntries.end();
          iter++ )
    {
      out << (*iter)->toString() << std::endl;
    }

    return out;
  }

  llvm::raw_ostream& ASTEntryList::toStream(llvm::raw_ostream& out)
  {
    std::stringstream ss;
    toStream(ss);
   
    out << ss.str(); 
    return out;
  }

  std::ostream& ASTEntryList::toStreamJSON(std::ostream& out )
  {
    out << toJSON() << std::endl;
    return out;
  }

  llvm::raw_ostream& ASTEntryList::toStreamJSON(llvm::raw_ostream& out)
  {
    std::stringstream ss;
    toStreamJSON(ss);

    out << ss.str();
    return out;
  }

  bool ASTEntryList::toFile(const std::string& outfilePath )
  {
    std::ofstream outfile( outfilePath.c_str() );
    return toStream(outfile).good();
  }

  bool ASTEntryList::toFileJSON(const std::string& outfilePath )
  {
    std::ofstream outfile( outfilePath.c_str() );
    return toStreamJSON(outfile).good();
  }
}

