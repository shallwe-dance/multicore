#include <iostream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <pthread.h>

#define BILLION  1000000000L

using namespace std;

unsigned int seed=time(NULL);

template<class K,class V,int MAXLEVEL>
class skiplist_node
{
public:
    skiplist_node()
    {
        for (int i = 1; i <= MAXLEVEL; i++) {
            forwards[i] = nullptr;
        }
    }

    skiplist_node(K searchKey):key(searchKey)
    {
        for (int i = 1; i <= MAXLEVEL; i++) {
            forwards[i] = nullptr;
        }
    }

    skiplist_node(K searchKey,V val):key(searchKey),value(val)
    {
        for (int i = 1; i <= MAXLEVEL; i++) {
            forwards[i] = nullptr;
        }
    }

    virtual ~skiplist_node()
    {
    }
    K key;
    V value;
    skiplist_node<K,V,MAXLEVEL>* forwards[MAXLEVEL+1];
};
///////////////////////////////////////////////////////////////////////////////

//automatically acquire and release pthread lock
//called by all operations
class Pthread_Automation {
public:
    explicit Pthread_Automation(pthread_mutex_t* mutex) : lock(mutex) {
	pthread_mutex_lock(lock);
    }

    ~Pthread_Automation() {
	pthread_mutex_unlock(lock);
    }
private:
    pthread_mutex_t* lock;   
};

template<class K, class V, int MAXLEVEL = 16>
class skiplist
{
public:
    typedef K KeyType;
    typedef V ValueType;
    typedef skiplist_node<K,V,MAXLEVEL> NodeType;

    skiplist(K minKey,K maxKey):m_pHeader(nullptr),m_pTail(nullptr),
                                max_curr_level(1),max_level(MAXLEVEL),
                                m_minKey(minKey),m_maxKey(maxKey)
    {
        m_pHeader = new NodeType(m_minKey);
        m_pTail   = new NodeType(m_maxKey);
        for (int i = 1; i <= MAXLEVEL; i++) {
            m_pHeader->forwards[i] = m_pTail;
        }
    }

    virtual ~skiplist()
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock in skiplist destructor
        NodeType* currNode = m_pHeader->forwards[1];
        while (currNode != m_pTail) {
            NodeType* tempNode = currNode;
            currNode = currNode->forwards[1];
            delete tempNode;
        }
        delete m_pHeader;
        delete m_pTail;

	pthread_mutex_destroy(&m_Mutex); //release mutex resource
    }

    void insert(K searchKey,V newValue)
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock when a method is called
        skiplist_node<K,V,MAXLEVEL>* update[MAXLEVEL+1];
        NodeType* currNode = m_pHeader;

        // find predecessor 
        for (int level = max_curr_level; level >= 1; level--) {
            while (currNode->forwards[level]->key < searchKey) {
                currNode = currNode->forwards[level];
            }
            update[level] = currNode;
        }
        currNode = currNode->forwards[1];

        if (currNode->key == searchKey) {
            currNode->value = newValue;
        } else {
            int newlevel = randomLevel();
            if (newlevel > max_curr_level) {
                for (int level = max_curr_level+1; level <= newlevel; level++) {
                    update[level] = m_pHeader;
                }
                max_curr_level = newlevel;
            }
            currNode = new NodeType(searchKey,newValue);
            for (int lv = 1; lv <= newlevel; lv++) {
                currNode->forwards[lv] = update[lv]->forwards[lv];
                update[lv]->forwards[lv] = currNode;
            }
        }
    }

    void erase(K searchKey)
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock when a method is called
        skiplist_node<K,V,MAXLEVEL>* update[MAXLEVEL+1];
        NodeType* currNode = m_pHeader;

        // find predecessor 
        for (int level = max_curr_level; level >= 1; level--) {
            while (currNode->forwards[level]->key < searchKey) {
                currNode = currNode->forwards[level];
            }
            update[level] = currNode;
        }

        currNode = currNode->forwards[1];

        if (currNode->key == searchKey) {
            for (int lv = 1; lv <= max_curr_level; lv++) {
                if (update[lv]->forwards[lv] == currNode) {
                    update[lv]->forwards[lv] = currNode->forwards[lv];
                }
            }
            delete currNode;

            while (max_curr_level > 1 && m_pHeader->forwards[max_curr_level] == m_pTail) {
                max_curr_level--;
            }
        }
    }

    bool find(K searchKey, V& outValue)
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock when a method is called
        NodeType* currNode = m_pHeader;
        for (int level = max_curr_level; level >= 1; level--) {
            while (currNode->forwards[level]->key < searchKey) {
                currNode = currNode->forwards[level];
            }
        }
        currNode = currNode->forwards[1];
        if (currNode->key == searchKey) {
            outValue = currNode->value;
            return true;
        }
        return false;
    }

    bool empty() const
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock when a method is called
        return (m_pHeader->forwards[1] == m_pTail);
    }

    std::string printList()
    {
	Pthread_Automation lock(&m_Mutex); //acquire lock when a method is called
        int i = 0;
        std::stringstream sstr;
        NodeType* currNode = m_pHeader->forwards[1];
        while (currNode != m_pTail) {
            sstr << currNode->key << " ";
            currNode = currNode->forwards[1];
            i++;
            if (i > 200) break;
        }
        return sstr.str();
    }

    const int max_level;

protected:
/*
    double uniformRandom()
    {
        return rand_r(&seed) / double(RAND_MAX);
    }
*/
    double uniformRandom()
    {
	thread_local static std::mt19937 generator(std::random_device{}());
	thread_local static std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return distribution(generator);
    }
    int randomLevel() {
        int level = 1;
        double p = 0.5;
        while (uniformRandom() < p && level < MAXLEVEL) {
            level++;
        }
        return level;
    }
    mutable pthread_mutex_t m_Mutex = PTHREAD_MUTEX_INITIALIZER;
    K m_minKey;
    K m_maxKey;
    int max_curr_level;
    skiplist_node<K,V,MAXLEVEL>* m_pHeader;
    skiplist_node<K,V,MAXLEVEL>* m_pTail;

};

