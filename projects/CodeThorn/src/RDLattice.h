#ifndef RD_LATTICE_H
#define RD_LATTICE_H

#include "Labeler.h"
#include "VariableIdMapping.h"
#include "Lattice.h"

namespace CodeThorn {

/*! 
  * \author Markus Schordan
  * \date 2013.
 */
class RDLattice : public Lattice {
 public:
  typedef std::set<std::pair<CodeThorn::Label,VariableId> >::iterator iterator;
  RDLattice();
  iterator begin() const;
  iterator end() const;
  size_t size() const;
  void insertPair(CodeThorn::Label,VariableId);
  void erasePair(CodeThorn::Label,VariableId);
  void removeAllPairsWithVariableId(VariableId var);
  bool isBot() const;
  void setBot();
  void toStream(std::ostream& os, VariableIdMapping* vim=0);
  bool exists(std::pair<CodeThorn::Label,VariableId>);
  void setEmptySet();
  void combine(Lattice& b);
  bool approximatedBy(Lattice& b) const;
  LabelSet getRDs(CodeThorn::VariableId);
 private:
  std::set<std::pair<CodeThorn::Label,VariableId> > rdSet;
  bool _bot;
};

}

#endif
