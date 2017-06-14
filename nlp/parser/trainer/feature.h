#ifndef NLP_PARSER_TRAINER_FEATURE_H_
#define NLP_PARSER_TRAINER_FEATURE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/registry.h"
#include "dragnn/protos/spec.pb.h"
#include "nlp/parser/trainer/shared-resources.h"
#include "nlp/parser/trainer/transition-state.h"

namespace sling {
namespace nlp {

// A Sempar feature function operates either in 'train' mode or in
// the normal extraction mode.
//
// In 'train' mode, the function is run via Train{Init,Process,Finish}
// methods, where it is expected to generate any resources (e.g. lexicons)
// and return the final domain size.
//
// In extraction mode, the feature is initialized using Init(), where it
// can load resources (e.g. the lexicons generated in the 'train' mode),
// and request any workspaces via RequestWorkspaces(), and perform the
// actual extraction via Preprocess() and Extract() methods.
//
// The feature function also has access to an integer argument (0 by default),
// as well as a map of parameter names/values.
class SemparFeature : public RegisterableClass<SemparFeature> {
 public:
  // Container for holding the input and outputs of feature extraction.
  struct Args {
    SemparState *state = nullptr;         // input state; not owned
    bool debug = false;                   // whether we are in debug mode
    std::vector<int64> output_ids;        // output feature ids
    std::vector<string> output_strings;   // corresponding feature debug strings

    // Short-cut accessors.
    syntaxnet::WorkspaceSet *workspaces() {
      return state->instance()->workspaces;
    }
    ParserState *parser_state() {
      return state->parser_state();
    }
    void Output(int64 id) {
      output_ids.emplace_back(id);
    }
  };

  virtual ~SemparFeature() {}

  // Accessors/mutators used while constructing the feature function.
  void SetParam(const string &name, const string &val) { params_[name] = val; }

  int GetIntParam(const string &name, int default_value) const;
  bool GetBoolParam(const string &name, bool default_value) const;
  float GetFloatParam(const string &name, float default_value) const;
  const string &GetParam(const string &name, const string &default_value) const;

  int argument() const { return arg_; }
  void set_argument(int a) { arg_ = a; }

  const string &name() const { return name_; }
  void set_name(const string &n) { name_ = n; }
  const string &fml() const { return fml_; }
  void set_fml(const string &f) { fml_ = f; }

  // Methods used to generate resources required by the feature function.
  // Only called during training.

  // Initializes any resources that will be constructed during training.
  // Any files should be written to 'output_folder'.
  virtual void TrainInit(
      SharedResources *resources, const string &output_folder) {}

  // Augments the under construction resource(s) using 'doc'.
  virtual void TrainProcess(const Document &doc) {}

  // Finishes resource construction and dumps their final path in 'spec'.
  // Returns the final domain size of the feature.
  virtual int TrainFinish(syntaxnet::dragnn::ComponentSpec *spec) = 0;

  // Methods used during feature extraction.

  // Loads any requisite resources using file paths in 'spec'.
  virtual void Init(const syntaxnet::dragnn::ComponentSpec &spec,
                    SharedResources *resources) {}

  // Requests any necessary workspaces.
  virtual void RequestWorkspaces(syntaxnet::WorkspaceRegistry *registry) {}

  // Preprocesses 'state'.
  virtual void Preprocess(SemparState *state) {}

  // Extracts features using 'args'.
  virtual void Extract(Args *args) = 0;

  // Returns the string representation of feature id 'id' (used in debugging).
  virtual string FeatureToString(int64 id) const = 0;

  // Gets the file path of the resource named 'name' in 'spec'.
  static string GetResource(const syntaxnet::dragnn::ComponentSpec &spec,
                            const string &name);

  // Adds a resource named 'name' with path 'file' to 'spec'.
  static void AddResourceToSpec(const string &name,
                                const string &file,
                                syntaxnet::dragnn::ComponentSpec *spec);

 private:
  // Feature name, e.g. "word".
  string name_;

  // Full FML of the feature, e.g. "word(-1)".
  string fml_;

  // Argument for the feature, e.g. -1 in "word(-1)".
  int arg_ = 0;

  // Any other parameters of the feature.
  std::unordered_map<string, string> params_;
};

#define REGISTER_SEMPAR_FEATURE(name, component) \
    REGISTER_CLASS_COMPONENT(SemparFeature, name, component);

// Manager for feature functions. It groups features into channels.
class SemparFeatureExtractor {
 public:
  ~SemparFeatureExtractor();

  // Methods for adding a channel of features.
  void AddChannel(const string &name,
                  const string &fml,
                  int embedding_dim);
  void AddChannel(const syntaxnet::dragnn::FixedFeatureChannel &channel);
  void AddChannel(const syntaxnet::dragnn::LinkedFeatureChannel &channel);

  // Trains the current set of channels using training data in 'train_files'.
  // If 'fill_vocabulary_size' is true then final domain sizes of features are
  // dumped into 'spec'.
  void Train(const std::vector<string> &train_files,
             const string &output_folder,
             bool fill_vocabulary_size,
             SharedResources *resources,
             syntaxnet::dragnn::ComponentSpec *spec);

  // Methods for feature extraction.
  void Init(
      const syntaxnet::dragnn::ComponentSpec &spec, SharedResources *resources);
  void RequestWorkspaces(syntaxnet::WorkspaceRegistry *registry);
  void Preprocess(SemparState *state);
  void Extract(SemparFeature::Args *args);

 private:
  // Represents one channel of features.
  struct Channel {
    string name;                             // channel name
    int embedding_dim = -1;                  // embedding dimensionality
    int vocabulary = -1;                     // max domain size
    std::vector<SemparFeature *> features;   // features in the channel
  };

  // Channels.
  std::vector<Channel> channels_;
};

}  // namespace nlp
}  // namespace sling

#endif // NLP_PARSER_TRAINER_FEATURE_H_
