/*
 * Copyright (C) 2005 Universitat d'Alacant / Universidad de Alicante
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include <lttoolbox/transducer.h>
#include <lttoolbox/compression.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/lttoolbox_config.h>
#include <lttoolbox/my_stdio.h>

#include <cstdlib>
#include <iostream>
#include <vector>


int
Transducer::newState()
{
  int nstate = transitions.size();

  while(transitions.find(nstate) != transitions.end())
  {
    nstate++;
  }  
  transitions[nstate].clear();  // force creating
  
  return nstate;
}

Transducer::Transducer()
{
  initial = newState();
}

Transducer::~Transducer()
{
  destroy();
}

Transducer::Transducer(Transducer const &t)
{
  copy(t);
}

Transducer &
Transducer::operator =(Transducer const &t)
{
  if(this != &t)
  {
    destroy();
    copy(t);
  }
  return *this;
}

int
Transducer::insertSingleTransduction(int const tag, int const source)
{
  if(transitions.find(source) != transitions.end())
  {
    if(transitions[source].count(tag) == 1)
    {
      pair<multimap<int,int>::iterator, multimap<int,int>::iterator > range;
      range = transitions[source].equal_range(tag);
      return range.first->second;
    }
    else if(transitions[source].count(tag) == 0)
    {
      // new state
      int state = newState();
      transitions[source].insert(pair<int, int>(tag, state));
      return state;
    }
    else if(transitions[source].count(tag) == 2)
    {
      // there's a local cycle, must be ignored and treated like in '1'
      pair<multimap<int,int>::iterator, multimap<int,int>::iterator> range;
      range = transitions[source].equal_range(tag);
      for(; range.first != range.second; range.first++)
      {
        if(range.first->second != source)
        {
          return range.first->second;
        }
      }
      return -1; 
    } 
    else
    {
      return -1;
    }
  }
  else
  {
    return -1;
  }
}

int
Transducer::insertNewSingleTransduction(int const tag, int const source)
{
  int state = newState();
  transitions[source].insert(pair<int, int>(tag, state));
  return state;
}

int
Transducer::insertTransducer(int const source, Transducer &t, 
                                 int const epsilon_tag)
{
  map<int, int> relacion;

  t.joinFinals(epsilon_tag);
  
  for(map<int, multimap<int, int> >::const_iterator it = t.transitions.begin(),
                                                    limit = t.transitions.end();
      it != limit; it++)
  {
    relacion[it->first] = newState();
  }

  for(map<int, multimap<int, int> >::const_iterator it = t.transitions.begin();
      it != t.transitions.end(); it++)
  {
    for(multimap<int, int>::const_iterator it2 = it->second.begin(),
                                           limit2 = (it->second).end(); 
        it2 != limit2; it2++)
    {
      transitions[relacion[it->first]].insert(pair<int, int>(it2->first, relacion[it2->second]));
    }
  }

  transitions[source].insert(pair<int, int>(epsilon_tag, 
					     relacion[t.initial]));

  return relacion[*(t.finals.begin())];
}

void
Transducer::linkStates(int const source, int const destino, 
			    int const etiqueta)
{

  if(transitions.find(source) != transitions.end() &&
     transitions.find(destino) != transitions.end())
  {
    // new code
    pair<multimap<int, int>::iterator, multimap<int, int>::iterator> range;
    range = transitions[source].equal_range(etiqueta);
    for(;range.first != range.second; range.first++)
    {
      if(range.first->first == etiqueta && range.first->second == destino)
      {
        return;
      }
    }
    // end of new code
    transitions[source].insert(pair<int, int>(etiqueta, destino));
  }
  else
  {
    wcerr << L"Error: Trying to link nonexistent states (" << source;
    wcerr << L", " << destino << L", " << etiqueta << L")" << endl;
    exit(EXIT_FAILURE);
  }
}

bool
Transducer::isFinal(int const state) const
{
  return finals.find(state) != finals.end();
}

void
Transducer::setFinal(int const state, bool valor)
{
  int initial_copy = getInitial();
/*
  if(state == initial_copy)
  {
    wcerr << L"Setting initial state to final" << endl;
  }
*/
  if(valor)
  {
    finals.insert(state);
  }
  else
  {
    finals.erase(state);
  }
}

int
Transducer::getInitial() const
{
  return initial;
}

set<int>
Transducer::closure(int const state, int const epsilon_tag)
{
  set<int> nonvisited, result;
  
  nonvisited.insert(state);
  result.insert(state);

  while(nonvisited.size() > 0)
  {
    int auxest = *nonvisited.begin();
    pair<multimap<int, int>::iterator, multimap<int, int>::iterator> rango;
    rango = transitions[auxest].equal_range(epsilon_tag);
    while(rango.first != rango.second)
    {
      if(result.find(rango.first->second) == result.end())
      {
        result.insert(rango.first->second);
        nonvisited.insert(rango.first->second);
      }
      rango.first++;
    }
    nonvisited.erase(auxest);
  }

  return result;
}

void
Transducer::joinFinals(int const epsilon_tag)
{
  if(finals.size() > 1)
  {
    int state = newState();

    for(set<int>::iterator it = finals.begin(), limit = finals.end(); 
        it != limit; it++)
    {
      linkStates(*it, state, epsilon_tag);
    } 

    finals.clear();
    finals.insert(state); 
  }
  else if(finals.size() == 0)
  {
    wcerr << L"Error: empty set of final states" <<endl;
    exit(EXIT_FAILURE);
  }
}

bool
Transducer::isEmptyIntersection(set<int> const &s1, set<int> const &s2)
{

  if(s1.size() < s2.size())
  {
    for(set<int>::const_iterator it = s1.begin(), limit = s1.end(); it != limit; it++)
    {
      if(s2.find(*it) != s2.end())
      {
	return false;
      }
    }    
  }
  else
  {
    for(set<int>::const_iterator it = s2.begin(), limit = s2.end(); it != limit; it++)
    {
      if(s1.find(*it) != s1.end())
      {
        return false;
      }
    }
  }

  return true;
}

void
Transducer::determinize(int const epsilon_tag)
{
  vector<set<int> > R(2);
  map<int, set<int> > Q_prima;
  map<set<int>, int> Q_prima_inv;

  map<int, multimap<int, int> > transitions_prima;

  unsigned int talla_Q_prima = 0;
  Q_prima[0] = closure(initial, epsilon_tag);

  Q_prima_inv[Q_prima[0]] = 0;
  R[0].insert(0);

  int initial_prima = 0;
  set<int> finals_prima;

  if(finals.find(initial) != finals.end())
  {
    finals_prima.insert(0);
  }
 
  int t = 0;

  while(talla_Q_prima != Q_prima.size())
  {
    talla_Q_prima = Q_prima.size();
    R[(t+1)%2].clear();
    
    for(set<int>::iterator it = R[t].begin(), limit = R[t].end(); 
        it != limit; it++)
    {
      if(!isEmptyIntersection(Q_prima[*it], finals))
      {
	finals_prima.insert(*it);
      }

      map<int, set<int> > mymap;      

      for(set<int>::iterator it2 = Q_prima[*it].begin(), 
                             limit2 = Q_prima[*it].end();
	  it2 != limit2; it2++)
      {
        for(multimap<int, int>::iterator it3 = transitions[*it2].begin(),
                                         limit3 = transitions[*it2].end();
	    it3 != limit3; it3++)
	{
	  if(it3->first != epsilon_tag)
	  {
	    set<int> c = closure(it3->second, epsilon_tag);
           
	    for(set<int>::iterator it4 = c.begin(), limit4 = c.end(); 
	        it4 != limit4; it4++)
	    {
	      mymap[it3->first].insert(*it4);
	    }
	  }
	}
      }

      // adding new states  
      for(map<int, set<int> >::iterator it2 = mymap.begin(), limit2 = mymap.end(); 
	  it2 != limit2; it2++)
      {   
	if(Q_prima_inv.find(it2->second) == Q_prima_inv.end())
	{
	  int etiq = Q_prima.size();
	  Q_prima[etiq] = it2->second;
	  Q_prima_inv[it2->second] = etiq;
	  R[(t+1)%2].insert(Q_prima_inv[it2->second]);
          transitions_prima[etiq].clear();          
	}
        transitions_prima[*it].insert(pair<int, int>(it2->first, Q_prima_inv[it2->second]));
      }
    } 

    t = (t+1)%2;
  }

  transitions = transitions_prima;
  finals = finals_prima;
  initial = initial_prima;
}


void
Transducer::minimize(int const epsilon_tag)
{
  reverse(epsilon_tag);
  determinize(epsilon_tag);
  reverse(epsilon_tag);
  determinize(epsilon_tag);
}

void
Transducer::optional(int const epsilon_tag)
{
  joinFinals(epsilon_tag);
  int state = newState();  
  linkStates(state, initial, epsilon_tag);
  initial = state;

  state = newState();
  linkStates(*finals.begin(), state, epsilon_tag);
  finals.clear();
  finals.insert(state);
  linkStates(initial, state, epsilon_tag);
}

void
Transducer::oneOrMore(int const epsilon_tag)
{
  joinFinals(epsilon_tag);
  int state = newState();
  linkStates(state, initial, epsilon_tag);
  initial = state;

  state = newState();
  linkStates(*finals.begin(), state, epsilon_tag);
  finals.clear();
  finals.insert(state);
  linkStates(state, initial, epsilon_tag);
}

void
Transducer::zeroOrMore(int const epsilon_tag)
{
  oneOrMore(epsilon_tag);
  optional(epsilon_tag);
}

void
Transducer::clear()
{
  finals.clear();
  transitions.clear();
  initial = newState();
}

bool
Transducer::isEmpty() const
{
  return finals.size() == 0 && transitions.size() == 1;
}

int
Transducer::size() const
{
  return transitions.size();
}

int
Transducer::numberOfTransitions() const
{
  int counter = 0;

  for(map<int, multimap<int, int> >::const_iterator it = transitions.begin(),
                                                    limit = transitions.end();
      it != limit; it++)
  {
    counter += (it->second).size();
  }

  return counter;
}

bool
Transducer::isEmpty(int const state) const
{
  map<int, multimap<int, int> >::const_iterator it;

  it = transitions.find(state);
  if(it != transitions.end())
  {
    if((it->second).size() > 0)
    {
      return false;
    }
  }

  return true;
}

void
Transducer::write(FILE *output, int const decalage)
{
  Compression::multibyte_write(initial, output);
  Compression::multibyte_write(finals.size(), output);

  int base = 0;
  for(set<int>::iterator it = finals.begin(), limit = finals.end(); 
      it != limit; it++)
  {
    Compression::multibyte_write(*it - base, output);
    base = *it;
  }

  base = transitions.size();
  Compression::multibyte_write(base, output);
  for(map<int, multimap<int, int> >::iterator it = transitions.begin(),
                                              limit = transitions.end();
      it != limit; it++)
  {
    Compression::multibyte_write(it->second.size(), output);
    int tagbase = 0;
    for(multimap<int, int>::iterator it2 = it->second.begin(),
                                     limit2 = it->second.end();
        it2 != limit2; it2++)
    {
      Compression::multibyte_write(it2->first-tagbase+decalage, output);
      tagbase = it2->first; 

      if(it2->second >= it->first)
      {
        Compression::multibyte_write(it2->second-it->first, output);
      }
      else
      {
        Compression::multibyte_write(it2->second+base-it->first, output);
      }
    }
  }
}

void
Transducer::read(FILE *input, int const decalage)
{
  Transducer new_t;

  new_t.initial = Compression::multibyte_read(input);
  int finals_size = Compression::multibyte_read(input);

  int base = 0;
  while(finals_size > 0)
  {
    finals_size--;

    base += Compression::multibyte_read(input);
    new_t.finals.insert(base);
  }

  base = Compression::multibyte_read(input);
  int number_of_states = base;
  int current_state = 0; 
  while(number_of_states > 0)
  {
    int number_of_local_transitions = Compression::multibyte_read(input);
    int tagbase = 0;
    while(number_of_local_transitions > 0)
    {
      number_of_local_transitions--;
      tagbase += Compression::multibyte_read(input) - decalage;
      int state = (current_state + Compression::multibyte_read(input)) % base;
      if(new_t.transitions.find(state) == new_t.transitions.end())
      {
        new_t.transitions[state].clear(); // force create
      }
      new_t.transitions[current_state].insert(pair<int, int>(tagbase, state));
    }    
    number_of_states--;
    current_state++;
  }

  *this = new_t;
}

void
Transducer::copy(Transducer const &t)
{
  initial = t.initial;
  finals = t.finals;
  transitions = t.transitions;
}

void
Transducer::destroy()
{
}

void
Transducer::reverse(int const epsilon_tag)
{
  joinFinals(epsilon_tag);

  map<int, multimap<int, int> > temporal;
  
  for(map<int, multimap<int, int> >::reverse_iterator it = transitions.rbegin(); it != transitions.rend(); it++)
  {
    multimap<int, int> aux = it->second;
    it->second.clear();
    for(multimap<int, int>::iterator it2 = aux.begin(), limit2 = aux.end(); 
        it2 != limit2; it2++)
    {
      if(it2->second >= it->first)
      {
        transitions[it2->second].insert(pair<int, int>(it2->first, it->first));
      }
      else
      {
        temporal[it2->second].insert(pair<int, int>(it2->first, it->first));
      }
    }
    if(temporal.find(it->first) != temporal.end())
    {
      (it->second).insert(temporal[it->first].begin(), temporal[it->first].end());
      temporal.erase(it->first);
    } 
  }

  for(map<int, multimap<int, int> >::reverse_iterator it = temporal.rbegin(), 
                                                      limit = temporal.rend(); 
      it != limit; it++)
  {
    for(multimap<int, int>::iterator it2 = it->second.begin(),
                                     limit2 = it->second.end();
	it2 != limit2; it2++)
    {
      transitions[it->first].insert(pair<int, int>(it2->first, it2->second));
    }
  } 
  
  int tmp = initial;
  initial = *(finals.begin());
  finals.clear();
  finals.insert(tmp);
}

void
Transducer::show(Alphabet const &alphabet, FILE *output, int const epsilon_tag)
{
  joinFinals(epsilon_tag);

  map<int, multimap<int, int> > temporal;

  for(map<int, multimap<int, int> >::iterator it = transitions.begin(); it != transitions.end(); it++)
  {
    multimap<int, int> aux = it->second;
  
    for(multimap<int, int>::iterator it2 = aux.begin(); it2 != aux.end(); it2++) 
    {
      pair<int, int> t = alphabet.decode(it2->first);
      fwprintf(output, L"%d\t", it->first);
      fwprintf(output, L"%d\t", it2->second);
      wstring l = L"";
      alphabet.getSymbol(l, t.first);
      if(l == L"")  // If we find an epsilon
      {
        fwprintf(output, L"ε\t", l.c_str());
      }
      else 
      {
        fwprintf(output, L"%S\t", l.c_str());
      }
      wstring r = L"";
      alphabet.getSymbol(r, t.second);
      if(r == L"")  // If we find an epsilon
      {
        fwprintf(output, L"ε\t", r.c_str());
      }
      else 
      {
        fwprintf(output, L"%S\t", r.c_str());
      }
      fwprintf(output, L"\n");
    } 
  } 

  for(set<int>::iterator it3 = finals.begin(); it3 != finals.end(); it3++)
  {
    fwprintf(output, L"%d\n", *it3);
  }
}

int 
Transducer::getStateSize(int const state)
{
 set<int> states;
 set<int> myclosure1 = closure(state, 0);
 states.insert(myclosure1.begin(), myclosure1.end());
 int num_transitions = 0;

 for(set<int>::iterator it2 = states.begin(); it2 != states.end(); it2++)
 {
   num_transitions += transitions[*it2].size();
 }

 return num_transitions;
}

bool
Transducer::recognise(wstring patro, Alphabet &a, FILE *err)
{
  bool accepted = false;
  set<int> states ;

  set<int> myclosure1 = closure(getInitial(), 0); 
  states.insert(myclosure1.begin(), myclosure1.end()); 
  // For each of the characters in the input string
  for(wstring::iterator it = patro.begin(); it != patro.end(); it++)  
  {
    set<int> new_state;        //Transducer::closure(int const state, int const epsilon_tag)
    int sym = *it;
    // For each of the current alive states
    //fwprintf(err, L"step: %S %C (%d)\n", patro.c_str(), *it, sym);
    for(set<int>::iterator it2 = states.begin(); it2 != states.end(); it2++)
    {
      multimap<int, int> p = transitions[*it2];
      // For each of the transitions in the state 

      for(multimap<int, int>::iterator it3 = p.begin(); it3 != p.end(); it3++)
      { 
        
	pair<int, int> t = a.decode(it3->first);
        wstring l = L"";
        a.getSymbol(l, t.first);
        //wstring r = L"";
        //a.getSymbol(r, t.second);

        //fwprintf(err, L"  -> state: %d, trans: %S:%S, targ: %d\n", *it2, (l == L"") ?  L"ε" : l.c_str(),  (r == L"") ?  L"ε" : r.c_str(), it3->second);
        //if(l.find(*it) != wstring::npos || l == L"" )
        if(l.find(*it) != wstring::npos)
        {
          set<int> myclosure = closure(it3->second, 0);
          //wcerr << L"Before closure alives: " <<new_state.size() << endl;  
          new_state.insert(myclosure.begin(), myclosure.end());
          //wcerr << L"After closure alives: " <<new_state.size() << endl;  
        }
      }
    }
    states = new_state;
  }
  for(set<int>::iterator it4 = states.begin(); it4 != states.end(); it4++)
  {
    if(isFinal(*it4)) 
    {
      accepted = true;
    }
  }

  return accepted;
}

void
Transducer::unionWith(Alphabet &my_a,
  Transducer &t,
  int const epsilon_tag)
{
  /*int tmp = newState();
  linkStates(tmp, initial, my_a(epsilon_tag, epsilon_tag));
  initial = tmp;*/
  finals.insert(insertTransducer(initial, t, epsilon_tag));
}

Transducer
Transducer::appendDotStar(set<int> const &loopback_symbols, int const epsilon_tag)
{
  Transducer prefix_transducer(*this);

  for(set<int>::iterator prefix_it = prefix_transducer.finals.begin(),
                           prefix_limit = prefix_transducer.finals.end();
      prefix_it != prefix_limit;
      prefix_it++)
  {
    for(set<int>::iterator loopback_it = loopback_symbols.begin(),
                           loopback_limit = loopback_symbols.end();
    loopback_it != loopback_limit;
    loopback_it++)
    {
      if((*loopback_it) != epsilon_tag) // TODO: Necessary? Minimization should remove epsilon loopbacks anyway
      {
        prefix_transducer.linkStates(*prefix_it, *prefix_it, *loopback_it);
      }
    }
  }

  return prefix_transducer;
}

Transducer
Transducer::intersect(Transducer &trimmer,
  Alphabet const &this_a,
  Alphabet const &trimmer_a,
  int const epsilon_tag)
{
//#define DEBUG  
  joinFinals(epsilon_tag);
  /**
   * this ∩ trimmer = trimmed
   * 
   * The trimmer is typically a bidix passed through appendDotStar.
   */

  // TODO: What to do about <g/>? Currently, "a<n># b" is added if
  // "a<n>" is in trimmer.

  // State numbers may differ in this transducer and the trimmed:
  Transducer trimmed;
  map<int, int> states_this_trimmed;
  states_this_trimmed.insert(make_pair(initial, trimmed.initial));

  Alphabet show_should_probably_accept_const_a = this_a;

  // first: currently searched state in this; second: current live states in trimmer
  typedef std::pair<int, set<int> > SearchState;

  std::list<SearchState> todo;
  std::set<std::pair<int, int> > seen;
  
  SearchState current;
  current.first = initial;
  current.second.insert(trimmer.initial);
  todo.push_front(current);

  while(todo.size() > 0)
  {
    current = todo.front();
    todo.pop_front();
    int this_src = current.first;
    set<int> live_trimmer_states = current.second;
    // Loop through arcs from this_src; when our arc matches an arc
    // from live_trimmer_states, add that to (the front of) todo:
    for(multimap<int, int>::iterator trans_it = transitions[this_src].begin(),
                                     trans_limit = transitions[this_src].end();
        trans_it != trans_limit;
        trans_it++)
    {
      int this_label = trans_it->first,
          this_trg   = trans_it->second;
      SearchState next, next_from_src;
      // Loop through live states in our trimmer transducer:
      for(set<int>::iterator trimmer_state_it = live_trimmer_states.begin(),
                             trimmer_state_limit = live_trimmer_states.end();
          trimmer_state_it != trimmer_state_limit;
          trimmer_state_it++)
      {
        int trimmer_src = (*trimmer_state_it);
        // Loop through arcs from each live state of trimmer:
        for(multimap<int, int>::iterator trimmer_trans_it
            = trimmer.transitions.at(trimmer_src).begin(),
                                         trimmer_trans_limit
            = trimmer.transitions.at(trimmer_src).end();
          trimmer_trans_it != trimmer_trans_limit;
          trimmer_trans_it++)
        {
          int trimmer_label = trimmer_trans_it->first,
              trimmer_trg   = trimmer_trans_it->second;
          wstring this_right = L"",
                  trimmer_left = L"";
          this_a.getSymbol(this_right,
                           this_a.decode(this_label).second);
          trimmer_a.getSymbol(trimmer_left,
                              trimmer_a.decode(trimmer_label).first);
#ifdef DEBUG
          wstring this_left = L"",
                  trimmer_right = L"";
          this_a.getSymbol(this_left, this_a.decode(this_label).first);
          trimmer_a.getSymbol(trimmer_right,
                              trimmer_a.decode(trimmer_label).second);
          wcerr << this_src
                << L"\t"
                << this_trg
                << L"\t"
                << (this_left == L"" ? L"ε" : this_left)
                << L"\t"
                << (this_right == L"" ? L"ε" : this_right)
                << L"\tis ";
#endif /* DEBUG */


          if(trimmer_left == L"" && this_right != L"") 
          {
            // Add a new live_trimmer_state from this_src, like
            // staying put in this FST
            if(seen.find(make_pair(this_src, trimmer_trg)) == seen.end()) 
            {
              next_from_src.second.insert(trimmer_trg);
            }
          }
          else if(   this_right == trimmer_left
                  || this_right == L"+" // TODO: use COMPILER_JOIN_ELEM from compiler.cc
                  || this_right == L"<compound-only-L>" // TODO: use compoundOnlyLSymbol
                  || this_right == L"<compound-R>" // TODO: use compoundRSymbol
                  || this_right == L""             // epsilon
            )
          {
            if(this_right == L"+")
            {
              trimmer_trg = trimmer.initial;
            }
            else if(    this_right == L"<compound-only-L>"
                    ||  this_right == L"<compound-R>"
                    || (this_right == L"" && trimmer_left != L"") )
            {
              // Stay put in the trimmer FST
              trimmer_trg = trimmer_src;
            }

            if(seen.find(make_pair(this_trg, trimmer_trg)) == seen.end()) 
            {
              next.second.insert(trimmer_trg);
            }
            int trimmed_src = states_this_trimmed[this_src];
            if(states_this_trimmed.find(this_trg) == states_this_trimmed.end())
            {
              states_this_trimmed.insert(make_pair(this_trg, trimmed.newState()));
            }
            int trimmed_trg = states_this_trimmed[this_trg];
            trimmed.linkStates(trimmed_src, // fromState
                               trimmed_trg, // toState
                               this_label); // symbol-pair, using this alphabet
#ifdef DEBUG
            wcerr << L"    ";
#endif /* DEBUG */
          }
#ifdef DEBUG
          else
          {
            wcerr << L"not ";
          }
          wcerr << L"equal to\t"
                << trimmer_src
                << L"\t "
                << trimmer_trg
                << L"\t "
                << (trimmer_left == L"" ? L"ε" : trimmer_left)
                << L"\t"
                << (trimmer_right == L"" ? L"ε" : trimmer_right)
                <<endl;
#endif /* DEBUG */
        }

        seen.insert(make_pair(this_src, trimmer_src));
      } // end loop live trimmer states

      if(next_from_src.second.size() > 0)
      {
        next_from_src.first = this_src;
        todo.push_front(next_from_src);
      }
      if(next.second.size() > 0)
      {
        next.first = this_trg;
        todo.push_front(next);
      }
    } // end loop arcs from this_src
  } // end while todo


  for(set<int>::iterator it = finals.begin(),
        limit = finals.end();
      it != limit;
      it++)
  {
    trimmed.finals.insert(states_this_trimmed[*it]);
  }


#ifdef DEBUG
  wcerr << L"Done trimming!\nInitial state: " << trimmed.getInitial()<<endl;
  trimmed.show(show_should_probably_accept_const_a);
  wcerr << L"running trimmed.minimize() ...";
#endif /* DEBUG */

  // minimize the trimmed transducer
  trimmed.minimize();

  return trimmed;
}
