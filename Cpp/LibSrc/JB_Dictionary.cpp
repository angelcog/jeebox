
// Copyright, Theodore H. Smith 2019.
// Released under jeebox-licence http://jeebox.org/licence.txt



// Headers
#include "JB_Umbrella.h"
#include <stdlib.h>


extern "C" {
    
struct Range {
    int Start;
    int Last;
    
    int Width() { return 1 + Last - Start; }
    Range (int a, int b) {
        Start = Min(a, b);
        Last = Max(a, b);
    }
    Range () {
    }
    Range Take(int i) {
        Range Result;
        Result.Start = Min(Start, i);
        Result.Last = Max(Last, i);
        return Result;
    }
    void Adjust (int W) {
        if (W + Start > 256) {
            Start = 256 - W;
        }
    }
};


int Width_(Dictionary0* Found) {
    return ((int)Found->Width) + 1; // sigh
}



static int Type_(JB_Object* Obj) {
    return ((IntPtr)Obj) & 0x03;
}

static bool IsValue_(JB_Object* Obj) {
    return Type_(Obj) == 0;
}
static bool IsInt_(JB_Object* Obj) {
    return Type_(Obj) == 1;
}
static bool IsLeaf_(JB_Object* Obj) {
    return Type_(Obj) == 2;
}
static bool IsBranch_(JB_Object* Obj) {
    return Type_(Obj) == 3;
}
static bool IsSomeDict_(JB_Object* Obj) {
    return Type_(Obj) & 0x02; 
}

static JB_Object* WasBranch_(Dictionary* Obj) {
    return (JB_Object*) (((IntPtr)Obj) | 0x03);
}

static JB_Object* WasLeaf_(DictionaryLeaf* Obj) {
    return (JB_Object*) (((IntPtr)Obj) | 0x02);
}

static JB_Object* WasInt_(u64 Num) {
    return (JB_Object*) ((Num << 2) | 0x01);
}

static Dictionary* Dict_(JB_Object* Obj) {
    return (Dictionary*) ((IntPtr)(Obj) &~ 0x03);
}

static JB_Object* Obj_(JB_Object* Obj) {
    return (JB_Object*) ((IntPtr)(Obj) &~ 0x03);
}

static DictionaryLeaf* Leaf_(JB_Object* Obj) {
    return (DictionaryLeaf*) ((IntPtr)(Obj) &~ 0x03);
}



void DictValueRemove_(JB_Object** Place) {
    // take over refcounting, to allow ints in here.
    JB_Object* Item = *Place;
    if (!Item) {return;}
    *Place = 0; // just be safe?
    JB_Object* B = Obj_(Item);
    if (Item == B) {                    // a normal object
        if (--B->RefCount) { return; }
    } else {
        if (IsInt_(Item)) { return; }
        B->RefCount = 0;
    }
    JB_Delete((FreeObject*)B);
}


static MiniStr LeafKey (DictionaryLeaf* Leaf) {
    return Mini(Leaf->Key, Leaf->Depth);
}

inline FindResult BeginFind_(Dictionary* self, MiniStr key) {
    FindResult F;
    F.Obj = WasBranch_(self);
    if (!self->Width) {self->Width = 255;}
    F.Place = 0;
    F.Data = key;
    F.Parent = 0;
    return F;
}




Dictionary* DictAlloc_(Dictionary0* Parent, int Width) {
    // just allocate... for at least that amount
    if (Width >= 1) {
        Dictionary* Result = 0;
        if (Width <= kDictLength1) {
            Result = (Dictionary*)JB_New(Dictionary1);
            Width = kDictLength1;
            
        } else if (Width <= kDictLength2) {
            Result = (Dictionary*)JB_New(Dictionary2);
            Width = kDictLength2;
            
        } else if (Width <= kDictLength3) {
            Result = (Dictionary*)JB_New(Dictionary3);
            Width = kDictLength3;
            
        } else if (Width <= kDictLength4) {
            Result = (Dictionary*)JB_New(Dictionary4);
            Width = kDictLength4;
            
        } else if (Width <= kDictLength5) {
            Result = (Dictionary*)JB_New(Dictionary);
            Width = kDictLength5; // lol :)
            
        } else {
            debugger;
            return 0;
        }

//        Result->NoRefCounts = Parent->NoRefCounts;
        Result->Width = Width - 1;
        for (int i = 0; i < Width; i++) {
            Result->Items[i] = 0;
        }
        
        JB_Incr(Result);
        return Result;
    }
    
    debugger;
    return 0;
}


static void NodeSanity_(Dictionary0* D) {
#if DEBUG
    dbgexpect(IsValue_(D));                                // is normal really....
    dbgexpect( D->Start + Width_(D) <= 256 ); // wat?

    Dictionary* P = D->Parent;
    if (P) {
        dbgexpect( IsValue_(P) );
        dbgexpect( P->Depth == D->Depth - 1 );
        dbgexpect( Dict_(P->Items[ D->Letter - P->Start ]) == D );
    }
#endif
}


static inline bool AccessItem_(FindResult& F) {
    Dictionary* Found = Dict_(F.Obj);
    NodeSanity_(Found);
    u8 i = *F.Data.Addr++;
    u32 Offset = i - (u32)Found->Start;

    if (Offset <= (u32)(Found->Width)) {
        F.Parent = Found;
        F.Place = &Found->Items[Offset];
        F.Obj = *F.Place;
        F.Data.Length--;
        return IsBranch_(F.Obj);
    }

// We can only use the byte if the position of the byte exists!
    F.Data.Addr--;
    return false;
}


static void BranchesOnly_( FindResult& F ) {
// Just... goes as far as possible.
    while (F.Data.Length >= 1) {
        if (!AccessItem_( F )) {
            return;
        }
    }
}


static void GoInLeaf_(FindResult& F, DictionaryLeaf* Leaf) {
    NodeSanity_(Leaf);
    F.Parent = (Dictionary*)Leaf;
    F.Place = &Leaf->Value;
    F.Obj = *F.Place;
}


static void ReadLeaf_(FindResult& F) {
    if ( IsLeaf_(F.Obj) ) {
        DictionaryLeaf* Leaf = Leaf_(F.Obj);
        
        // Just compare what remains
        MiniStr Key = LeafKey(Leaf);
        if (StrEquals(Key, F.Data)) {
            GoInLeaf_(F, Leaf);
            F.Data.Addr  += F.Data.Length;
            F.Data.Length = 0;
        }
    }
}


static void UseInPlaceValue_( FindResult& Found ) {
    // Assume in branch.
    if ( IsSomeDict_(Found.Obj) ) {
        Dictionary* D = Dict_(Found.Obj);
        Found.Parent = D;
        Found.Place = &(D->InPlaceValue);
        Found.Obj = *Found.Place;
    }
}


static void RangeFix_(Dictionary0* D) {
    int S = D->Start;
    int W = Width_(D);
    int N = S + W;
    int Over = N - 256;
    if (Over > 0) {
        D->Start = S - Over;
    }
}


JB_String20*  TheDictName; // don't use this for set!
static JB_String* NotSharedDict_(JB_String* Key) {
    if (Key != TheDictName) {
        return Key;
    }
    return JB_Str_Copy( Key );
}

static void WriteLeaf_(FindResult& F, JB_String* Key) {
    // should be easy...
    dbgexpect(IsValue_(F.Parent)); // not a value but just not shifted.
    if (F.Data.Length >= 2) {
        DictionaryLeaf* Leaf = JB_New(DictionaryLeaf);
        JB_Incr(Leaf); // I think so!
        
        *F.Place = WasLeaf_(Leaf);
        Leaf->InPlaceValue = F.Obj;
        Leaf->Depth = F.Data - Key->Addr; // must come first!
        Key = NotSharedDict_(Key);
        Leaf->Key = (JB_String*)(JB_Incr(Key));
        Leaf->Start = 0;
        Leaf->Width = 0;
        Leaf->Parent = (Dictionary*)F.Parent;
        Leaf->Letter = F.Data[-1];

        F.Data.Length = 0;
        F.Place = &Leaf->Value;
        *F.Place = 0;
        F.Obj = 0;
        NodeSanity_(Leaf);

    } else if (F.Data.Length) {
        Dictionary* D = DictAlloc_(F.Parent, 1);
        
        *F.Place = WasBranch_(D);
        D->InPlaceValue = F.Obj;
        D->Depth = F.Data - Key->Addr;
        D->Letter = F.Data[-1];
        D->Parent = (Dictionary*)F.Parent;
        D->Start = *F.Data.Addr++;
        RangeFix_(D);

        F.Data.Length--;
        F.Parent = D;
        F.Obj = 0;
        F.Place = &D->Items[F.Data[-1] - D->Start];
        NodeSanity_(D);
    }
    dbgexpect( F.Data.Length == 0);
}

    
static void WriteItem_(FindResult& F, JB_String* NewKey, Dictionary* P) {
    // write place into F... May be a leaf, branch or value...
    MiniStr S = Mini(NewKey, P->Depth);
    F.Parent = P;
    if (!S.Length) {
        F.Place = &P->InPlaceValue;
        F.Obj = *F.Place;
    } else {
        F.Place = &(P->Items[S[0] - P->Start]);
        F.Obj = *F.Place;
        if (S.Length >= 2) {
            dbgexpect(!IsSomeDict_(F.Obj));
            F.Data = S + 1;
            WriteLeaf_(F, NewKey);
        } else if (IsSomeDict_(F.Obj)) {
            F.Place = &Dict_(F.Obj)->InPlaceValue;
            F.Obj = *F.Place;
        }
    }
}

// OK so let's re-write... transform this into something that... simply unleafifies...
// and THEN... simply adds the old strings back in! is it easier, though?
// how do we even do this?
Dictionary* UnLeafify_(FindResult& F, DictionaryLeaf* Leaf) {
// Mismatch in a leaf. Replace everything with branches up to the first mismatch.
    MiniStr OldKey = LeafKey(Leaf);
    int DiffAt = StrDiffAt( OldKey, F.Data );
    Dictionary* P = Leaf->Parent;
    JB_Object** Place = F.Place;

    dbgexpect2( DiffAt != 0 );
    dbgexpect2( F.Parent == P );
    dbgexpect2( IsValue_(F.Parent) );
    if (DiffAt < 0) {
        DiffAt = (-DiffAt) - 1;        // OK so one is smaller...
    }

    // So we need to create a chain... of branches... Each with only 1 child!
    for (int c = 0; c < DiffAt; c++) {
        Range R = Range( F.Data[c], OldKey[c] );
//        R.Start = R.Start &~ 0x07;              // OK so... let's... hopefully... make the new one... have a "step" of 8?
        Dictionary* B = DictAlloc_( P, R.Width() );
        B->Parent = P;
        B->Depth = P->Depth + 1;
        B->Letter = F.Data[c - 1];
        B->Start = R.Start;
        RangeFix_(B); // remove above tests? remove Range.Adjust? or just use range.adjust cos it's needed for copybranch.
        B->InPlaceValue = Leaf->InPlaceValue;
        Leaf->InPlaceValue = 0;
        *Place = WasBranch_(B);

        P = B;
        Place = &(B->Items[F.Data[c] - B->Start]);       // neither seem to solve this? Which one is correct?
        NodeSanity_(P);
    }
    
    return P;
}
    

static void ReplaceLeaf_(FindResult& F, JB_String* NewKey) {
    DictionaryLeaf* Leaf = Leaf_(F.Obj);
    Dictionary* P = UnLeafify_(F, Leaf);
    
    JB_Object* LeafValue = Leaf->Value;
    Leaf->Value = 0;
    WriteItem_(F, Leaf->Key, P);
    *F.Place = LeafValue;
    JB_Decr(Leaf);
    
    
    WriteItem_(F, NewKey, P);
    F.Data.Addr += F.Data.Length;
    F.Data.Length = 0; // sigh.
}


Range CurrRange_(Dictionary* Found) {
    for ( int i = 0;  i < Width_(Found);  i++ )  {
        if (Found->Items[i]) {
            int Last = i;
            for ( int j = i + 1 ; j < Width_(Found); j++) {
                if (Found->Items[j]) {
                    Last = j;
                }
            }

            // OK so... it's from i to Last. seems OK?
            return { Found->Start + i, Found->Start + Last };
        }
    }

    return {0, 0};
}


static void CopyBranch_(Dictionary* D1, Range R1, Dictionary* D2, Range R2) {
    int N2 = Width_(D2);                   // need to erase values before and after...
    int N1 = Width_(D1);                   // need to alter the node's new parent!
    for (int i = 0; i < N2; i++) {
        int Letter = i + R2.Start;
        int PosD1 = Letter - D1->Start;
        Dictionary0* Node = 0;
        if ((u32)PosD1 < (u32)N1) {
            Node = (Dictionary0*)D1->Items[PosD1];
            if (IsSomeDict_(Node)) {
                Dict_(Node)->Parent = D2;
            }
        }
        D2->Items[i] = Node;
    }
    
    D2->Start = R2.Start;
    D2->Letter = D1->Letter;
    D2->Depth = D1->Depth;
    D2->InPlaceValue = D1->InPlaceValue;
    D2->Parent = D1->Parent;
}



static void Copy_(Dictionary* S, Dictionary* D) {
    NodeSanity_(S);
    int N = 4 * Width_(S);
    memzero(D->Items, 1024);
    memmove( D->Items + S->Start, S->Items, N );
    dbgexpect( D->Items[S->Start] == S->Items[0] );
    D->Letter = S->Letter;
    D->Parent = S->Parent;
}

static void Compare_(Dictionary* New, Dictionary* Old) {
    // So we have two dictionaries... test that everything in the new is in the old...
    // and that nothing else... exists.
    for (int Before = 0; Before < New->Start; Before++) {
        dbgexpect( !Old->Items[Before] );
    }
    for (int After = New->Start + Width_(New); After < 256; After++) {
        dbgexpect( !Old->Items[After] );
    }
    for (int i = 0; i < Width_(New); i++) {
        JB_Object* N = New->Items[i];
        int Letter = i + New->Start;
        JB_Object* O = Old->Items[Letter];
        dbgexpect( O == N );
        if (IsSomeDict_(N)) {
            Dictionary* D = Dict_(N);
            dbgexpect( D->Parent == New );
        }
    }
}


static void AdjustBranch_(FindResult& F) {
    dbgexpect(F.Place);
    Dictionary* Found = Dict_(F.Obj);
    Range O = CurrRange_(Found);
    Range N = O.Take(*F.Data.Addr);
    N.Start = N.Start &~ 0x07;              // OK so... let's... hopefully... make the new one... have a "step" of 8?

    Dictionary* Bigger = DictAlloc_(F.Parent, N.Width());
    N.Adjust(Width_(Bigger));
    CopyBranch_(Found, O, Bigger, N);
    *F.Place = WasBranch_(Bigger);
    F.Obj = Bigger;
    Found->Width = 0;                       // Disable destructor
    JB_Decr(Found);
}



void JB_Dict_Value_(FindResult& F) {
    BranchesOnly_(F);
    if (F.Data.Length) {
        ReadLeaf_(F);
    } else {
        UseInPlaceValue_(F);
    }
}


bool TrySet_(FindResult& F, JB_String* Key) {
    dbgexpect2(F.Place);
    if (!F.Data.Length) {
        return true;
    }
    
    // Branch, value, or leaf!
    if (IsBranch_(F.Obj)) {             // 1) Branch: Not big enough
        AdjustBranch_(F);
        BranchesOnly_(F); // right?
    }
    
    if (!F.Data.Length) {
        return true;
    }
    if (!IsSomeDict_(F.Obj)) {      // 2) Value : Replace with leaf.
        WriteLeaf_(F, Key);

    } else if (IsLeaf_(F.Obj)) {
        ReplaceLeaf_(F, Key);       // 3) Leaf  : Mismatch somewhere...

    } else {
        debugger; // does this even ever happen?
    }
    
    return !F.Data.Length;
}



bool CanFind_(Dictionary* self, JB_String* key, FindResult* F) {
    if (key and self) {
        *F = BeginFind_( self, Mini(key, 0) );
        JB_Dict_Value_(*F);
        return !F->Data.Length;
    }
    return false;
}


bool CanSet_(Dictionary* self, JB_String* key, FindResult* F) {
    if (key and self) {
        *F = BeginFind_( self,  Mini(key, 0) );
        JB_Dict_Value_(*F);
        return TrySet_(*F, key);
    }
    return false;
}


JB_String* JB_Str_UniqueSplit(JB_String* self, int StartOff, int Length, Dictionary* D) {
    MiniStr S = {Length, self->Addr + StartOff};
    FindResult F = BeginFind_( D, S );
    JB_Dict_Value_(F);
    if (!F.Data.Length and F.Obj) { // Found
        return (JB_String*)F.Obj;
    }    
    
    // not found, so... copy, set, and return? makes sense?
    JB_String* key = JB_Str_CopyFromPtr(S.Addr, S.Length);
    if (!TrySet_(F, key)) {
        debugger;
        JB_SetRef(key, 0);
    }
    return key;
    // OK, so we set it. Now... return it?
}




JB_Object** JB_Dict_MakePlace(Dictionary* self, JB_String* key) {
    FindResult F;
    if (CanSet_(self, key, &F)) {
        return F.Place;
    }
    return 0;
}


static JB_Object* IntStore_(u64 V) {
    JB_Object* Stored = (JB_Object*)WasInt_(V);
    if (((u64)Stored >> 2)==V) {
        return Stored;
    }
    
    // need to wrap...
    Stored = JB_New(IntWrapper);
    if (Stored) {
        Stored->RefCount = 1;
        ((IntWrapper*)Stored)->Value = V;
    }
    return Stored; 
}


bool ObjIsInt_(JB_Object* Obj) {
    return (JB_ObjLayer(Obj) == &(JB_AsClass(IntWrapper)->Memory));
}


s64 JB_Dict_IntValue(Dictionary* self, JB_String* key, s64 Missing) {
    FindResult F;
    if (CanFind_(self, key, &F)) {
        JB_Object* B = F.Obj;
        if (B) {
            if (IsInt_(B)) {
                return ((IntPtr)B) >> 2;
            }
            
            if (ObjIsInt_(B)) {
                return ((IntWrapper*)B)->Value;
            }
        }
    }
    return Missing;
}


void JB_Dict_IntValueSet(Dictionary* self, JB_String* key, s64 Value) {
    FindResult F;
    if (CanSet_(self, key, &F)) {
        *F.Place = IntStore_(Value);
        if (IsValue_(F.Obj)) { // could be a direct int
            JB_Decr(F.Obj);
        }
    }
}



JB_Object* JB_Dict_Value(Dictionary* self, JB_String* key) {
    FindResult F;
    if (CanFind_(self, key, &F)) {
        JB_Object* B = F.Obj;
        if (!IsInt_(B)) {
            return B;
        }
    }
    return 0;
}


void JB_Dict_ValueSet(Dictionary* self, JB_String* key, JB_Object* Value) {
    FindResult F;
    if (CanSet_(self, key, &F)) {
        *F.Place = JB_Incr(Value);
        if (IsValue_(F.Obj)) { // could be a direct int
            JB_Decr(F.Obj);
        }
    }
}


inline void Take_(ObjLength& self, JB_Object* Obj, int Length) {
    if (Obj) {
        self.Obj = Obj;
        self.Length = Length;
    }
}

ObjLength JB_Dict_LongestKey_( Dictionary* Curr, MiniStr Data ) {
    ObjLength Result = {Curr->InPlaceValue, 0};
    for ( int i = 0; i < Data.Length; ) {
        u32 C = Data.Addr[i++] - Curr->Start;
        if (C < Width_(Curr)) {
            JB_Object* Item = Curr->Items[C];
            if (!IsSomeDict_(Item)) {
                Take_(Result, Item, i);
            } else {
                Curr = Dict_(Item);
                Take_(Result, Curr->InPlaceValue, i);
                if (IsBranch_(Item)) {
                    continue;
                }
                MiniStr Key = LeafKey((DictionaryLeaf*)Curr); // So... how to compare? I guess let's make a smaller string?
                MiniStr SmallerData = {Min(Key.Length, Data.Length), Data.Addr + i};
                if (StrEquals(Key, SmallerData)) {
                    Take_(Result, ((DictionaryLeaf*)Curr)->Value, SmallerData.Length + i);
                }
            }
        }
        break;
    }
    
    return Result;
}





    ////////////////////////     Navigator     /////////////////////////

DictionaryReader* JB_Dict_Navigate( Dictionary* Dict ) {
    if (Dict) {
        DictionaryReader* Result = JB_New( DictionaryReader );
        JB_Nav_Constructor(Result, Dict);
        return Result;
    }
    return 0;
}



JB_Object* JB_Dict_Scan_(Dictionary* self, MiniStr Data, MiniStr* Result) {
    u8* Read = Data.Addr;
    u8* ReadEnd = Read + Data.Length;
    
    for ( ; Read < ReadEnd; Read++ ) {
        ObjLength Found = JB_Dict_LongestKey_( self, {(int)(ReadEnd-Read), Read} );
        if ( Found.Obj ) {
            Result->Addr = Read;
            Result->Length = Found.Length;
            return Found.Obj;
        }
    }
    
    Result->Addr = ReadEnd;
    Result->Length = 0;
    
    return 0;
}



void JB_Dict_Dispose(Dictionary* self) {
    JB_Dict_Destructor( self );
    JB_Dict_Constructor( self );
}


void JB_Leaf_Destructor(DictionaryLeaf* self) {
    JB_Decr(self->Key);
    DictValueRemove_(&self->InPlaceValue);
    DictValueRemove_(&self->Value);
}


JB_String* JB_Dict_Render(Dictionary* self, FastString* fs_in) {
	//visible;
	FastString* fs = JB_FS__FastNew(fs_in);
    DictionaryReader* Nav = JB_Dict_Navigate(self);
	JB_FS_AppendByte(fs, '[');
    bool Started = false;
    while (JB_Nav_MoveNext(Nav)) {
        if (Started) {
            JB_FS_AppendByte(fs, ',');
            JB_FS_AppendByte(fs, ' ');
        }
        Started = true;
        
        JB_String* S = JB_Nav_Name(Nav);
        JB_FS_AppendString(fs, S);
        JB_FreeIfDead(S);
        JB_FS_AppendByte(fs, ':');
        JB_ObjRender(JB_Nav_Value(Nav), fs);
    };
	JB_FS_AppendByte(fs, ']');
    JB_FreeIfDead(Nav);
	return JB_FS_SmartResult(fs, fs_in);
}



void JB_Dict_Destructor(Dictionary* self) {
    if (!self->Width) { // has to come first. I'm sure there's some code around here that explains why?
        return;
    }
    DictValueRemove_(&self->InPlaceValue);

    for (int i = Width_(self) - 1; i >= 0; i--) {
        DictValueRemove_(&self->Items[i]);
        self->Items[i] = (JB_Object*) - 1;       // garbage.
    }

    self->InPlaceValue = (JB_Object*) -1;        // garbage.
    self->Parent = (Dictionary*) -1;             // garbage.
}


void JB_Dict_Constructor(Dictionary* self) {
    JB_Zero( self );
    self->Width = 255;
}



//  //  ////////////////////////////////////////////////////////////////  //  //

static bool IsLeaf2_(Dictionary0* P) {
    return P and !P->Width;
}
static DictionaryLeaf* IsFinalLeafValue(FindResult& F) {
    DictionaryLeaf* Leaf = (DictionaryLeaf*)F.Parent;
    if (F.Place == &Leaf->Value) {
        return Leaf;
    }
    dbgexpect2(&Leaf->InPlaceValue == F.Place);
    return 0;
}

static bool Child_(FindResult& F) {
    if (IsLeaf2_(F.Parent)) { // leaves have no width
        if (IsFinalLeafValue(F)) {
            return false;
        }
        GoInLeaf_(F, (DictionaryLeaf*)F.Parent);
        return true;
    }
    
    if (!IsSomeDict_(F.Obj)) {
        return false;
    }

    UseInPlaceValue_(F);
    return true;
}


static void Up_(FindResult& F) {
    Dictionary0* P = F.Parent;
    Dictionary* P2 = P->Parent;

    F.Parent = P2;
    F.Obj = P;
    F.Place = 0;
    if (P2) {
        F.Place = P2->Items + P->Letter - P2->Start;
    }
}

static bool Next_(FindResult& F) {
    Dictionary0* P = F.Parent;
    if ( IsLeaf2_(P) ) {
        return false;
    }

    JB_Object** Items = ((Dictionary*)P)->Items;
    int Pos = (int)(F.Place - Items); if (Pos < -1) {debugger;};
    Pos = Max(Pos, -1); // remove? its -1 min anyhow for inplacevalue.
    while ( ++ Pos < Width_(P) ) {
        F.Place = &Items[Pos];
        F.Obj = *F.Place;
        if (F.Obj) {
            UseInPlaceValue_(F);
            return true;
        }
    }
    
    return false;
}


bool MoveNext_(FindResult& F) {
    if (Child_(F)) {
        return true;
    }
    
    while (F.Parent) {
        if ( Next_(F) ) {
            return true;
        }
        
        Up_(F);
    }
    
    return false;
}


bool JB_Nav_MoveNext(DictionaryReader* self) {
    if (!self) {return false;}
    FindResult& State = self->State;
    
    while (MoveNext_(State)) {
        if (State.Obj) {
            return true;
        }
    }
    
    return false;
}


JB_Object* JB_Nav_Value(DictionaryReader* self) {
    JB_Object* B = self->State.Obj; 
    dbgexpect2( !IsSomeDict_(B) );
    if (IsInt_(B)) {
        return 0; // sigh
    }
    return B;
}


void JB_Nav_ValueSet(DictionaryReader* self, JB_Object* V) {
    JB_Incr(V);
    JB_Decr(JB_Nav_Value(self));
    *(self->State.Place) = V;
}


bool JB_Nav_HasInt(DictionaryReader* self, s64* Where) {
    JB_Object* B = self->State.Obj; 
    if (IsInt_(B)) {
        *Where = ((IntPtr)B) >> 2;
    } else if (ObjIsInt_(B)) {
        *Where = ((IntWrapper*)B)->Value;
    } else {
        return false;
    }
    return true;
}


JB_String* JB_Nav_Name(DictionaryReader* self) {
    FindResult& F = self->State;
    if (IsLeaf2_(F.Parent)) {
        DictionaryLeaf* Leaf = IsFinalLeafValue(F);
        if (Leaf) { // OK so... return directly?
            return Leaf->Key; // !!
        }
    }

    Dictionary* P = (Dictionary*)F.Parent;
    int Extra = 0;
    if (F.Place != &P->InPlaceValue) {
        Extra = 1; // so we also need to write that last letter in...
    }
    JB_String* Result = JB_Str_New(P->Depth + Extra);
    if (Result and Result->Length) {
        u8* Write = Result->Addr + Result->Length;
        if (Extra) {
            *--Write = P->Start + (F.Place - P->Items);
        }
        while (P->Parent) {
            *--Write = P->Letter;
            P = P->Parent;
        }
    }
    
    return Result;
}


void JB_Nav_Destructor( DictionaryReader* self ) {
    // nothing
}

void JB_Nav_Constructor( DictionaryReader* self, Dictionary* Dict ) {
    MiniStr Tmp = { 0, 0 };
    self->State = BeginFind_( Dict, Tmp );
}





void JB_Dict__Init() {
    TheDictName = (JB_String20*)JB_Str_New(20);
    TheDictName->RefCount = 2;
    TheDictName->Addr = TheDictName->Data;
}


JB_String* JB_DictName_Trim (JB_String20* D, int N) {
    u8* Addr = D->Data;
    while (!Addr[N-1] and --N>1) {};
    D->Length = N;
    return D;
}

JB_String* JB_Obj_DictName (JB_Object* obj) {
    JB_String20* D = TheDictName;
    *((JB_Object**)D->Data) = obj;
    return JB_DictName_Trim(D, sizeof(obj));
}

JB_String* JB_int_DictName (int obj) {
    JB_String20* D = TheDictName;
    *((int*)D->Data) = obj;
    return JB_DictName_Trim(D, sizeof(obj));
}

JB_String* JB_s64_DictName (s64 obj) {
    JB_String20* D = TheDictName;
    *((s64*)D->Data) = obj;
    return JB_DictName_Trim(D, sizeof(obj));
}


    
//// TESTS

void JB_Dict_MainTests() {
    Dictionary* self = JB_New(Dictionary);
    JB_Dict_Constructor(self);
    JB_Incr(self);

    JB_String* s7 = JB_Str("\\n");
    JB_String* s6 = JB_Str("\\");
    JB_String* s5 = JB_Str("Operator_As_Thing");
    JB_String* s4 = JB_Str("Operator");
    JB_String* s3 = JB_Str("abcxyz");
    JB_String* s2 = JB_Str("abcdef");
    JB_String* s1 = JB_Str("abc123");
    JB_Incr(s7); JB_Incr(s6); JB_Incr(s5); JB_Incr(s4); JB_Incr(s3); JB_Incr(s2); JB_Incr(s1);


    JB_Dict_IntValueSet(self, s6, (IntPtr)s6);
    JB_Dict_IntValueSet(self, s7, (IntPtr)s7);
    dbgexpect(JB_Dict_IntValue(self, s6,-1) == (IntPtr)s6);
    dbgexpect(JB_Dict_IntValue(self, s7,-1) == (IntPtr)s7);
    
    JB_Dict_IntValueSet(self, s1, (IntPtr)s1);
    
    JB_Dict_IntValueSet(self, s2, (IntPtr)s2);
    dbgexpect(JB_Dict_IntValue(self, s1,-1) == (IntPtr)s1);
    
    JB_Dict_IntValueSet(self, s3, (IntPtr)s3);
    dbgexpect(JB_Dict_IntValue(self, s1,-1) == (IntPtr)s1);
    dbgexpect(JB_Dict_IntValue(self, s2,-1) == (IntPtr)s2);

    JB_Dict_IntValueSet(self, s4, (IntPtr)s4);
    JB_Dict_IntValueSet(self, s5, (IntPtr)s5);
    dbgexpect(JB_Dict_IntValue(self, s4,-1) == (IntPtr)s4);
    dbgexpect(JB_Dict_IntValue(self, s5,-1) == (IntPtr)s5);

    for (int i = -2; i < 100; i++) {
        JB_Dict_IntValueSet(self, s1, i);
        dbgexpect(JB_Dict_IntValue(self,s1,-1)==i);
        JB_Dict_IntValueSet(self, s1, kMaxint-i);
        dbgexpect(JB_Dict_IntValue(self,s1,-1)==(kMaxint-i));
    }
    
    JB_Dict_ValueSet(self, s2, s2);
    dbgexpect(JB_Dict_IntValue(self,s2,-1)==-1);
    JB_Dict_ValueSet(self, s1, s1);
    dbgexpect(JB_Dict_Value(self, s1) == s1);


    JB_Decr(s7); JB_Decr(s6); JB_Decr(s5); JB_Decr(s4); JB_Decr(s3); JB_Decr(s2); JB_Decr(s1);
    JB_Decr(self);
}




    
} // 



