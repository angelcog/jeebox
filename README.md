# jeebox

http://jeebox.org theo@jeebox.org

Compile using:

    cd </path/to/jeebox-master>
    cmake -BBuild; cd Build; sudo make

# About
Jeebox tries to fulfil the goal of describing everything. Here's an example of Jeebox syntax.

	\You can (% describe anything, in: Jeebox)

See real syntax examples here: http://jeebox.org/stuff/introduction. While Jeebox is useful for describing code, and save-files, and natural-language, the original goal was to allow describing the meaning_of_code.

Basically, Jeebox is a parser. So it parses source-code, and creates a tree. Just like an XML-parser, Jeebox is extensible, so it has no keywords. This means you can describe anything very easily.

Jeebox has a C-API and a C++-API. It parses UTF-8 only. The parse-speed is around the same as TinyXML2's parse-speed, it's pretty fast!

This is the first release so email any questions to theo@jeebox.org!


# Why use Jeebox?

Well... almost anything can use Jeebox. So the question might be "why not use Jeebox?" I've built a programming language around Jeebox, so I know it's really easy to use and fun to use. The language is beautiful, and I never feel "restricted", because I can always just use more of the existing Jeebox syntax. If you were writing a programming language, then using Jeebox means you basically don't need to write your own parser.

You just use Jeebox and get an incredible language with all sorts of features designed specifically for programming languages.

Jeebox can also describe human language, like English/Spanish/German/etc. I think writing a translation program (convert English/Spanish/etc to other languages) would be a great idea! This would help test Jeebox's use for describing meaning, and I'd get a chance to improve Jeebox's syntax "real world meanings". Only minor improvements would be needed, perhaps none.

Jeebox has incredible potential when it comes to translation, because it captures the grammatical structure of a sentance, it can allow for near-perfect translations. And the syntax is very light, considering the amount of grammatical structure it describes!
    
Jeebox isn't a static thing, it will be modified slightly over time. But on the whole (I've been using it for 7 years) it's barely changed much, just a few additions, almost no changes that break anything. Jeebox is pretty stable and mature by now!

Jeebox has a lot of potential. It could become sort of like "The Unicode of programming languages". (I know that's just an idealistic version of reality! I know reality doesn't work like that.) I did say "potential".


# Notes
For the moment Jeebox is 32-bit only. However the tests work and you can compile projects with it. I am working on a 64-bit version. My 32-bit version made some assumptions that aren't valid in 64-bits.

The Cpp/JB.cpp file is actually auto-generated by my compiler, `Speedie`. This means any changes to it, will be quite hard to merge back into the main source. Also, its pretty hard to understand because it's auto-generated.

Speedie takes Speedie source-code, and turns it into C. Almost all early compilers did this! Even C++ and Java did this once. Speedie isn't part of the distribution, but it's nice to know that Jeebox is being used for a big real-world project (Speedie is based on Jeebox).

I'll release Speedie at some point in the future anyhow. Releasing Jeebox was a big enough job!
