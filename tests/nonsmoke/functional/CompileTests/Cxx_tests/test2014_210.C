

template <int T>
class X
   {
     public:
         void free(X ptr) 
             {
               int i = T;
             }
   };

namespace {

     void foo()
        {
           X<5> a;
           a.free(a);
        }

}

