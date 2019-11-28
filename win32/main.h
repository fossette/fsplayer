//---------------------------------------------------------------------------
#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.Dialogs.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.OleCtrls.hpp>
//---------------------------------------------------------------------------
class TrfMain : public TForm
{
__published:	// IDE-managed Components
   TOpenDialog *rfOpen;
   TTimer *rrTimer;
   TPanel *rrView;
   void __fastcall FormCreate(TObject *Sender);
   void __fastcall rrTimerTimer(TObject *Sender);
   void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
   void __fastcall FormKeyDown(TObject *Sender, WORD &Key, TShiftState Shift);
private:	// User declarations
public:		// User declarations
   __fastcall TrfMain(TComponent* Owner);
   __fastcall ~TrfMain(void);
};
//---------------------------------------------------------------------------
extern PACKAGE TrfMain *rfMain;
//---------------------------------------------------------------------------
#endif
