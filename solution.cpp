#include <utility>

#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "progtest_solver.h"
#include "sample_tester.h"

using namespace std;
#endif /* __PROGTEST__ */

class OrderWrapper {
public:
    int cust_id;
    AOrderList orderList;

    OrderWrapper(int cust_id, const AOrderList & orderList) : cust_id(cust_id), orderList(orderList) {}
};

typedef std::shared_ptr<OrderWrapper> AOrderWrapper;

class SolveDatShit {
public:
    set<AProducer> producers;
    vector<APriceList> price_lists;
    bool ready = false;
    bool joined = false;

    mutex p_m_join, m_solve;
    condition_variable cv_solve;
    APriceList result;

    void join() {
        p_m_join.lock();
        if (!joined) {
            result = price_lists[0];

            for (auto & list :price_lists) {
                for (auto & item : list->m_List) {
                    bool duplicate = false;
                    for (auto & res : result->m_List) {
                        if ((item.m_H == res.m_H && item.m_W == res.m_W) ||
                            (item.m_H == res.m_W && item.m_W == res.m_H)) {
                            res.m_Cost = (item.m_Cost < res.m_Cost) ? item.m_Cost : res.m_Cost;
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) result->m_List.push_back(item);
                }
            }
            joined = true;
            cv_solve.notify_one();
        }
        cv_solve.notify_one();
        p_m_join.unlock();

    }

    void solve(AOrderList & orderList) {
        unique_lock<mutex> ul(m_solve);
        cv_solve.wait(ul, [this]() { return joined; });

        for (auto & order:orderList->m_List) {
            vector<COrder> ord({order});
            ProgtestSolver(ord, result);
            order.m_Cost = ord[0].m_Cost;
        }
        ul.unlock();

    }

    SolveDatShit(const AProducer & producer, const APriceList & price_list) {
        producers.insert(producer);
        price_lists.push_back(price_list);
    }

};

typedef std::shared_ptr<SolveDatShit> ASolver;


class CWeldingCompany {
public:

    vector<AProducer> producers;
    vector<ACustomer> customers;

    vector<thread> cts;
    vector<thread> wts;

    unordered_map<int, ASolver> price_lists;

    queue<AOrderWrapper> q_ao_list;

    size_t finish_cust = 0;
    mutex m_apl, m_svt, m_customer, m_svt1, mut;
    condition_variable cv_serv_t, cv_serv_t1;

    void CustomerThreadFunction(size_t id, ACustomer customer) {

        while (true) {
            AOrderWrapper orderWrapper = make_shared<OrderWrapper>(id, customer->WaitForDemand());

            //cv_customers.notify_one();
            if (orderWrapper->orderList == nullptr) {
                m_customer.lock();
                finish_cust++;
                if(finish_cust == customers.size())
                    cv_serv_t.notify_one();
                m_customer.unlock();
                return;
            }

            m_customer.lock();
            if (price_lists.find(orderWrapper->orderList->m_MaterialID) == price_lists.end()) {
                for (auto & prod:producers) {
                    prod->SendPriceList(orderWrapper->orderList->m_MaterialID); // void calls add price list
                }
            }
            q_ao_list.push(orderWrapper);
            cv_serv_t.notify_one();
            m_customer.unlock();
        }
    }

    void AddPriceList(AProducer prod, APriceList priceList) {

        if (price_lists.find(priceList->m_MaterialID) == price_lists.end()) {
            price_lists[priceList->m_MaterialID] = make_shared<SolveDatShit>(prod, priceList);
            cv_serv_t1.notify_one();
            if (producers.size() == 1) {
                m_apl.lock();
                price_lists[priceList->m_MaterialID]->ready = true;
                if (!price_lists[priceList->m_MaterialID]->joined) {
                    m_apl.unlock();
                    price_lists[priceList->m_MaterialID]->join();
                }
                m_apl.unlock();
            }
        }
        else {
            if (price_lists[priceList->m_MaterialID]->producers.find(prod) == price_lists[priceList->m_MaterialID]->producers.end()) {
                price_lists[priceList->m_MaterialID]->producers.insert(prod);
                price_lists[priceList->m_MaterialID]->price_lists.push_back(priceList);
                if (price_lists[priceList->m_MaterialID]->producers.size() == producers.size()) {
                    m_apl.lock();
                    price_lists[priceList->m_MaterialID]->ready = true;
                    if (!price_lists[priceList->m_MaterialID]->joined) {
                        m_apl.unlock();
                        price_lists[priceList->m_MaterialID]->join(); //     final_price_lists[priceList->m_MaterialID] =
                    }
                    m_apl.unlock();
                }
            }
        }
    }

    void ServiceThreadFunction(size_t id) {

        while (finish_cust != customers.size() || !q_ao_list.empty()) {
            unique_lock<mutex> ul(m_svt);//ul
            cv_serv_t.wait(ul, [this]() { return (finish_cust == customers.size() || !q_ao_list.empty()); });
            if (finish_cust == customers.size() && q_ao_list.empty()) {
                cv_serv_t.notify_one();
                break;
            }
            if (q_ao_list.front() == nullptr) {
                q_ao_list.pop();
                ul.unlock();
                continue;
            }

            if (price_lists.find(q_ao_list.front()->orderList->m_MaterialID) == price_lists.end()) {
                unique_lock<mutex> ul1(m_svt1);//ul
                cv_serv_t1.wait(ul1, [this]() { return price_lists.find(q_ao_list.front()->orderList->m_MaterialID) != price_lists.end(); });
                ul1.unlock();
            }
            AOrderWrapper orderList = q_ao_list.front();

            if (!q_ao_list.empty()) {
                q_ao_list.pop();
            }
            else continue;

            ul.unlock();
            price_lists[orderList->orderList->m_MaterialID]->solve(orderList->orderList);

            customers[orderList->cust_id]->Completed(orderList->orderList);
        }
        id = 0;
    }

    static void SeqSolve(APriceList priceList, COrder & order) { //static
        vector<COrder> ord({order});
        ProgtestSolver(ord, priceList);
        order.m_Cost = ord[0].m_Cost;
    };

    void AddProducer(AProducer prod) {
        producers.push_back(prod);
    }

    void AddCustomer(ACustomer cust) {
        customers.push_back(cust);
    }

    void Start(unsigned thrCount) {
        mut.lock();
        for (size_t id = 0; id < customers.size(); ++id) {
            cts.emplace_back(&CWeldingCompany::CustomerThreadFunction, this, id, customers[id]);
        }

        for (size_t id = 0; id < thrCount; ++id) {
            wts.emplace_back(&CWeldingCompany::ServiceThreadFunction, this, id);
        }
        mut.unlock();
    }

    void Stop(void) {
        mut.lock();
        for (auto & c_thrd : cts) {
            c_thrd.join();
        }
        for (auto & w_thrd : wts) {
            w_thrd.join();
        }
        mut.unlock();
    }


};


#ifndef __PROGTEST__

int main(void) {
    using namespace std::placeholders;
    clock_t tStart = clock();

    CWeldingCompany test;

    AProducer p1 = make_shared<CProducerSync>(bind(&CWeldingCompany::AddPriceList, &test, _1, _2));
    AProducerAsync p2 = make_shared<CProducerAsync>(bind(&CWeldingCompany::AddPriceList, &test, _1, _2));
     test.AddProducer(p1);
    test.AddProducer(p2);
    test.AddCustomer(make_shared<CCustomerTest>(100));
    test.AddCustomer(make_shared<CCustomerTest>(1));
    test.AddCustomer(make_shared<CCustomerTest>(1));
    test.AddCustomer(make_shared<CCustomerTest>(1));
    //test.AddCustomer(make_shared<CCustomerTest>(50));
    /* test.AddCustomer(make_shared<CCustomerTest>(100000));
     test.AddCustomer(make_shared<CCustomerTest>(100000));
     test.AddCustomer(make_shared<CCustomerTest>(100000));*/
    p2->Start();
    test.Start(2);
    test.Stop();
    p2->Stop();
    printf("Time taken: %.4fs\n", (double) (clock() - tStart) / CLOCKS_PER_SEC);

    return 0;
}

#endif /* __PROGTEST__ */
