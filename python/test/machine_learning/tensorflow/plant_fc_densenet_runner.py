# REF [paper] >> "Densely Connected Convolutional Networks", arXiv 2016.
# REF [paper] >> "The One Hundred Layers Tiramisu: Fully Convolutional DenseNets for Semantic Segmentation", arXiv 2016.
# REF [site] >> https://github.com/titu1994/Fully-Connected-DenseNets-Semantic-Segmentation
# REF [site] >> https://github.com/0bserver07/One-Hundred-Layers-Tiramisu

# Path to libcudnn.so.
#export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

#--------------------
import os, sys
if 'posix' == os.name:
	swl_python_home_dir_path = '/home/sangwook/work/SWL_github/python'
	lib_home_dir_path = '/home/sangwook/lib_repo/python'
else:
	swl_python_home_dir_path = 'D:/work/SWL_github/python'
	lib_home_dir_path = 'D:/lib_repo/python'
	#lib_home_dir_path = 'D:/lib_repo/python/rnd'
sys.path.append(swl_python_home_dir_path + '/src')
sys.path.append(lib_home_dir_path + '/Fully-Connected-DenseNets-Semantic-Segmentation_github')
#sys.path.append('../../../src')

#os.chdir(swl_python_home_dir_path + '/test/machine_learning/tensorflow')

#--------------------
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from plant_fc_densenet import PlantFcDenseNet
from swl.machine_learning.tensorflow.neural_net_trainer import NeuralNetTrainer
from swl.machine_learning.tensorflow.neural_net_evaluator import NeuralNetEvaluator
from swl.machine_learning.tensorflow.neural_net_predictor import NeuralNetPredictor
from swl.machine_learning.util import to_one_hot_encoding
from swl.image_processing.util import load_image_list_by_pil, generate_image_patch_list
import time

#np.random.seed(7)

#%%------------------------------------------------------------------
# Configure tensorflow.

config = tf.ConfigProto()
#config.allow_soft_placement = True
config.log_device_placement = True
config.gpu_options.allow_growth = True
#config.gpu_options.per_process_gpu_memory_fraction = 0.4  # Only allocate 40% of the total memory of each GPU.

# REF [site] >> https://stackoverflow.com/questions/45093688/how-to-understand-sess-as-default-and-sess-graph-as-default
#graph = tf.Graph()
#session = tf.Session(graph=graph, config=config)
session = tf.Session(config=config)

#%%------------------------------------------------------------------
# Prepare directories.

import datetime

output_dir_prefix = 'fc-densenet'
timestamp = datetime.datetime.now().strftime('%Y%m%dT%H%M%S')

model_dir_path = './result/{}_model_{}'.format(output_dir_prefix, timestamp)
model_dir_path = './result/fc-densenet_model_20180117T135317'
prediction_dir_path = './result/{}_prediction_{}'.format(output_dir_prefix, timestamp)
train_summary_dir_path = './log/{}_train_{}'.format(output_dir_prefix, timestamp)
val_summary_dir_path = './log/{}_val_{}'.format(output_dir_prefix, timestamp)

#%%------------------------------------------------------------------
# Load data.

if 'posix' == os.name:
	#data_home_dir_path = '/home/sangwook/my_dataset'
	data_home_dir_path = '/home/HDD1/sangwook/my_dataset'
else:
	data_home_dir_path = 'D:/dataset'

image_dir_path = data_home_dir_path + '/phenotyping/RDA/all_plants'
label_dir_path = data_home_dir_path + '/phenotyping/RDA/all_plants_foreground'

image_suffix = ''
image_extension = 'png'
label_suffix = '_foreground'
label_extension = 'png'

patch_height, patch_width = 224, 224

image_list = load_image_list_by_pil(image_dir_path, image_suffix, image_extension)
label_list = load_image_list_by_pil(label_dir_path, label_suffix, label_extension)

assert len(image_list) == len(label_list), '[SWL] Error: The numbers of images and labels are not equal.'
for (img, lbl) in zip(image_list, label_list):
	assert img.shape[:2] == lbl.shape[:2], '[SWL] Error: The sizes of every corresponding image and label are not equal.'

# For checking.
if False:
	fg_ratios = []
	for lbl in label_list:
		fg_ratios.append(np.count_nonzero(lbl) / lbl.size)

	small_image_indices = []
	for (idx, img) in enumerate(image_list):
		if img.shape[0] < patch_height or img.shape[1] < patch_width:
			small_image_indices.append(idx)

image_patches, label_patches = [], []
for (img, lbl) in zip(image_list, label_list):
	if img.shape[0] >= patch_height and img.shape[1] >= patch_width:
		img_pats, lbl_pats, _ = generate_image_patch_list(img, lbl, patch_height, patch_width, 0.02)
		if img_pats is not None and lbl_pats is not None:
			image_patches += img_pats
			label_patches += lbl_pats
			#patch_regions += pat_rgns

image_patches = np.array(image_patches)
label_patches = np.array(label_patches)
#patch_regions = np.array(image_patches)

train_images, test_images, train_labels, test_labels = train_test_split(image_patches, label_patches, test_size=0.2, random_state=None)

#--------------------
def preprocess_data(data, labels, num_classes, axis=0):
	if data is not None:
		# Preprocessing (normalization, standardization, etc.).
		data = data.astype(np.float32)
		data /= 255.0
		#data = (data - np.mean(data, axis=axis)) / np.std(data, axis=axis)
		#data = np.reshape(data, data.shape + (1,))

	if labels is not None:
		labels //= 255
		# One-hot encoding (num_examples, height, width) -> (num_examples, height, width, num_classes).
		labels = to_one_hot_encoding(labels, num_classes).astype(np.uint8)

	return data, labels

num_classes = 2
input_shape = (patch_height, patch_width, 3)
output_shape = (patch_height, patch_width, num_classes)

# Pre-process.
train_images, train_labels = preprocess_data(train_images, train_labels, num_classes)
test_images, test_labels = preprocess_data(test_images, test_labels, num_classes)

#%%------------------------------------------------------------------
# Create a model.

from keras import backend as K
K.set_learning_phase(1)  # Set the learning phase to 'train'.
#K.set_learning_phase(0)  # Set the learning phase to 'test'.
denseNetForPlant = PlantFcDenseNet(input_shape, output_shape)

print('[SWL] Info: Created a FC-DenseNet model.')

#%%------------------------------------------------------------------
# Train the model.

batch_size = 12  # Number of samples per gradient update.
num_epochs = 50  # Number of times to iterate over training data.

shuffle = True

#TRAINING_MODE = 0  # Start training a model.
TRAINING_MODE = 1  # Resume training a model.
#TRAINING_MODE = 2  # Use a saved model.

if 0 == TRAINING_MODE:
	initial_epoch = 0
	print('[SWL] Info: Start training...')
elif 1 == TRAINING_MODE:
	initial_epoch = 50
	print('[SWL] Info: Resume training...')
elif 2 == TRAINING_MODE:
	initial_epoch = 0
	print('[SWL] Info: Use a saved model.')
else:
	assert False, '[SWL] Error: Invalid TRAINING_MODE.'

nnTrainer = NeuralNetTrainer(denseNetForPlant, initial_epoch)
print('[SWL] Info: Created a trainer.')

session.run(tf.global_variables_initializer())

with session.as_default() as sess:
	# Save a model every 2 hours and maximum 5 latest models are saved.
	saver = tf.train.Saver(max_to_keep=5, keep_checkpoint_every_n_hours=2)

	if 1 == TRAINING_MODE or 2 == TRAINING_MODE:
		# Load a model.
		# REF [site] >> https://www.tensorflow.org/programmers_guide/saved_model
		# REF [site] >> http://cv-tricks.com/tensorflow-tutorial/save-restore-tensorflow-models-quick-complete-tutorial/
		ckpt = tf.train.get_checkpoint_state(model_dir_path)
		saver.restore(sess, ckpt.model_checkpoint_path)
		#saver.restore(sess, tf.train.latest_checkpoint(model_dir_path))

		print('[SWL] Info: Restored a model.')

	if 0 == TRAINING_MODE or 1 == TRAINING_MODE:
		start_time = time.time()
		history = nnTrainer.train(sess, train_images, train_labels, test_images, test_labels, batch_size, num_epochs, shuffle, saver=saver, model_save_dir_path=model_dir_path, train_summary_dir_path=train_summary_dir_path, val_summary_dir_path=val_summary_dir_path)
		print('\tTraining time = {}'.format(time.time() - start_time))

		# Display results.
		nnTrainer.display_history(history)

if 0 == TRAINING_MODE or 1 == TRAINING_MODE:
	print('[SWL] Info: End training...')

#%%------------------------------------------------------------------
# Evaluate the model.

print('[SWL] Info: Start evaluating...')

nnEvaluator = NeuralNetEvaluator()
print('[SWL] Info: Created an evaluator.')

with session.as_default() as sess:
	num_test_examples = 0
	if test_images is not None and test_labels is not None:
		if test_images.shape[0] == test_labels.shape[0]:
			num_test_examples = test_images.shape[0]

	if num_test_examples > 0:
		start_time = time.time()
		test_loss, test_acc = nnEvaluator.evaluate(sess, denseNetForPlant, test_images, test_labels, batch_size)
		end_time = time.time()

		print('\tTest loss = {}, test accurary = {}, evaluation time = {}'.format(test_loss, test_acc, end_time - start_time))
	else:
		print('[SWL] Error: The number of test images is not equal to that of test labels.')

print('[SWL] Info: End evaluating...')

#%%------------------------------------------------------------------
# Predict.

print('[SWL] Info: Start prediction...')

nnPredictor = NeuralNetPredictor()
print('[SWL] Info: Created a predictor.')

with session.as_default() as sess:
	num_pred_examples = 0
	if test_images is not None and test_labels is not None:
		if test_images.shape[0] == test_labels.shape[0]:
			num_pred_examples = test_images.shape[0]

	if num_pred_examples > 0:
		start_time = time.time()
		predictions = nnPredictor.predict(sess, denseNetForPlant, test_images, batch_size)
		end_time = time.time()

		predictions = np.argmax(predictions, -1)
		groundtruths = np.argmax(test_labels, -1)
		correct_estimation_count = np.count_nonzero(np.equal(predictions, groundtruths))

		print('\tAccurary = {} / {} = {}, prediction time = {}'.format(correct_estimation_count, groundtruths.size, correct_estimation_count / groundtruths.size, end_time - start_time))
	else:
		print('[SWL] Error: The number of test images is not equal to that of test labels.')

print('[SWL] Info: End prediction...')

#%%------------------------------------------------------------------
# Predict for full-size images.

from PIL import Image
import matplotlib.pyplot as plt

img, lbl = image_list[0], label_list[0]

ratio = max(patch_height / img.shape[0], patch_width / img.shape[1])
#img2 = np.asarray(Image.fromarray(img).resize((int(img.shape[1] * ratio), int(img.shape[0] * ratio)), resample=Image.BICUBIC))
img2 = np.asarray(Image.fromarray(img).resize((int(img.shape[1] * ratio), int(img.shape[0] * ratio)), resample=Image.LANCZOS))
lbl2 = np.asarray(Image.fromarray(lbl).resize((int(lbl.shape[1] * ratio), int(lbl.shape[0] * ratio)), resample=Image.NEAREST))

plt.figure()
plt.imshow(img)
plt.figure()
plt.imshow(lbl)
plt.figure()
plt.imshow(img2)
plt.figure()
plt.imshow(lbl2)

#%%

def to_one_hot(label_indexes, num_classes=None):
	return np.eye(num_classes)[label_indexes].reshape(label_indexes.shape + (-1,))

def find_most_frequent_value(a):
	counts = np.bincount(a)
	return np.argmax(counts)

np.argmax(a)

def stitch_patch_predictions(patches, regions, shape):
	pred = np.zeros(shape)
	for (pat, rgn) in zip(patches, regions):
		pred[rgn[0]:rgn[2],rgn[1]:rgn[3]] += pat

	patch_preds = np.asarray(Image.fromarray(patch_preds).resize((shape[1], shape[0]), resample=Image.NEAREST))
	return pred

print('[SWL] Info: Start prediction for full-size images...')

with session.as_default() as sess:
	start_time = time.time()
	for (img, lbl) in zip(image_list, label_list):
		if img.shape[0] < patch_height or img.shape[1] < patch_width:
			ratio = max(patch_height / img.shape[0], patch_width / img.shape[1])
			height, width = int(img.shape[0] * ratio), int(img.shape[1] * ratio)
			#img = np.asarray(Image.fromarray(img).resize((width, height), resample=Image.BICUBIC))
			img = np.asarray(Image.fromarray(img).resize((width, height), resample=Image.LANCZOS))
			lbl = np.asarray(Image.fromarray(lbl).resize((width, height), resample=Image.NEAREST))

		img_pats, _, pat_rgns = generate_image_patch_list(img, None, patch_height, patch_width, None)
		predictions = []
		if img_pats is not None and lbl_pats is not None and pat_rgns is not None:
			assert len(img_pats) == len(pat_rgns), '[SWL] Error: The number of generated patches is not equal.'

			patch_images, _ = preprocess_data(np.array(img_pats), None, num_classes)

			patch_preds = nnPredictor.predict(sess, denseNetForPlant, patch_images, batch_size=None)

			patch_preds = stitch_patch_predictions(patch_preds, pat_rgns, lbl.shape[:2])

			patch_preds = np.argmax(patch_preds, -1)
			predictions.append(patch_preds)
	end_time = time.time()

	predictions = np.array(predictions)

	num_pred_examples = 0
	if predictions is not None and test_labels is not None:
		if predictions.shape[0] == test_labels.shape[0]:
			num_pred_examples = predictions.shape[0]

	if num_pred_examples > 0:
		groundtruths = np.argmax(test_labels, -1)
		correct_estimation_count = np.count_nonzero(np.equal(predictions, groundtruths))

		print('\tAccurary = {} / {} = {}, prediction time = {}'.format(correct_estimation_count, groundtruths.size, correct_estimation_count / groundtruths.size, end_time - start_time))
	else:
		print('[SWL] Error: The number of test images is not equal to that of test labels.')

print('[SWL] Info: End prediction for full-size images...')
